#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mdt;

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams; ++i)
    {
        const auto& s = params()[(size_t) i];
        const auto p = (P) i;
        juce::NormalisableRange<float> range { s.min, s.max, s.step };
        if (s.logCurve)
            range = { s.min, s.max,
                      [s] (float, float, float v) { return toNatural (s, v); },
                      [s] (float, float, float x) { return toNormalised (s, x); },
                      [] (float lo, float hi, float x) { return juce::jlimit (lo, hi, x); } };
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name, range, s.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [p] (float v, int) { return juce::String (juce::CharPointer_UTF8 (format (p, v).c_str())); })));
    }
    return layout;
}
} // namespace

MiditinProcessor::MiditinProcessor()
    : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (params()[(size_t) i].id);
    for (auto* prm : getParameters())
        prm->addListener (this);

    setPatch ({});
    markClean();
}

void MiditinProcessor::setPatch (const Patch& p)
{
    patch = p;
    patches.back() = p;
    patches.publish();
    ++patchVersion;
    dirty = true;
}

void MiditinProcessor::post (UiEvent e)
{
    const juce::SpinLock::ScopedLockType sl (postLock);
    fifo.write (1).forEach ([&] (int i) { events[(size_t) i] = e; });
}

void MiditinProcessor::prepareToPlay (double sampleRate, int)
{
    // A re-prepare would forget what is sounding; note it so the next block can release it.
    UiSnapshot& s = snapshots.back();
    engine.fillSnapshot (s);
    for (int m = 0; m < 128; ++m)
        if (s.notes[(size_t) m].on)
            pendingOffs[(size_t) m] = s.notes[(size_t) m].ch;

    engine.prepare (sampleRate);
    preview.prepare (sampleRate);
    input.ensureSize (4096);
}

void MiditinProcessor::releaseResources() {}

bool MiditinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();   // the AUDIO energy source's input
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void MiditinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    bypassed = false;

    if (patches.fetch())
        audioPatch = patches.front();

    ParamValues pv;
    for (int i = 0; i < numParams; ++i)
        pv.v[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);

    Transport tr;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            tr.known = true;
            tr.playing = pos->getIsPlaying();
            tr.bpm = pos->getBpm().orFallback (0.0);
            tr.ppq = pos->getPpqPosition().orFallback (0.0);
        }

    input.clear();
    input.addEvents (midi, 0, n, 0);
    midi.clear();

    for (int m = 0; m < 128; ++m)
        if (const int ch = std::exchange (pendingOffs[(size_t) m], 0))
            midi.addEvent (juce::MidiMessage::noteOff (ch, m), 0);

    fifo.read (fifo.getNumReady()).forEach ([&] (int i) {
        const auto& e = events[(size_t) i];
        switch (e.type)
        {
            // The IN keyboard plays on CH, so a channel filter never drops it.
            case UiEvent::noteOn:      input.addEvent (juce::MidiMessage::noteOn (std::max (1, audioPatch.inChannel), e.note, e.vel), 0); break;
            case UiEvent::noteOff:     input.addEvent (juce::MidiMessage::noteOff (std::max (1, audioPatch.inChannel), e.note), 0); break;
            case UiEvent::releaseHeld: engine.releaseHeld (midi, 0); break;
            case UiEvent::kick:        engine.kick(); break;
        }
    });

    // The input is only measured (AUDIO energy source); nothing passes through.
    engine.process (audioPatch, pv, tr, input, midi, n, buffer.getArrayOfReadPointers(), getTotalNumInputChannels());
    buffer.clear();
    if (audioPatch.preview)
        preview.render (midi, buffer);
    else
        preview.reset();
    publishSnapshot();
}

void MiditinProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    // Bypass is deaf as well as mute: input note-offs are not seen while it lasts, so forget the
    // held notes now rather than keep a released note's poly voice running afterwards.
    if (! std::exchange (bypassed, true))
    {
        engine.releaseAll (midi, 0);
        engine.releaseHeld (midi, 0);
        preview.reset();
        publishSnapshot();
    }
}

void MiditinProcessor::publishSnapshot()
{
    engine.fillSnapshot (snapshots.back());
    snapshots.publish();
}

juce::AudioProcessorEditor* MiditinProcessor::createEditor() { return new MiditinEditor (*this); }

juce::ValueTree MiditinProcessor::captureDocument() const
{
    ParamValues pv;
    for (int i = 0; i < numParams; ++i)
        pv.v[(size_t) i] = raw[(size_t) i]->load();
    return makeDocument (patch, pv);
}

void MiditinProcessor::applyDocument (const juce::ValueTree& doc)
{
    Patch p;
    ParamValues pv;
    readDocument (doc, p, pv);
    // The preview is a property of where the plugin runs, not of the sound.
    p.preview = patch.preview;
    for (int i = 0; i < numParams; ++i)
        if (auto* prm = apvts.getParameter (params()[(size_t) i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (pv.v[(size_t) i]));
    setPatch (p);
}

void MiditinProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto root = captureDocument();
    root.setProperty ("openPresetKey", openPresetKey, nullptr);
    root.setProperty ("openPresetName", openPresetName, nullptr);
    root.setProperty ("preview", patch.preview, nullptr);
    root.setProperty ("editorWidth", editorWidth, nullptr);
    if (const auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void MiditinProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;
    const auto root = juce::ValueTree::fromXml (*xml);
    applyDocument (root);
    auto p = patch;
    p.preview = root.getProperty ("preview", p.preview);
    setPatch (p);
    openPresetKey = root["openPresetKey"].toString();
    openPresetName = root["openPresetName"].toString();
    editorWidth = juce::jlimit (675, 2250, (int) root.getProperty ("editorWidth", 900));
    markClean();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MiditinProcessor(); }
