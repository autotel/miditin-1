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
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (params()[(size_t) i].id);

    // The preview synth is the Standalone's only voice; in a DAW the MIDI goes to a real one.
    Patch p;
    p.preview = wrapperType == wrapperType_Standalone;
    setPatch (p);
}

void MiditinProcessor::setPatch (const Patch& p)
{
    patch = p;
    patches.back() = p;
    patches.publish();
    ++patchVersion;
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
        pendingOffs[(size_t) m] = pendingOffs[(size_t) m] || s.notes[(size_t) m].on;

    engine.prepare (sampleRate);
    preview.prepare (sampleRate);
    input.ensureSize (4096);
}

void MiditinProcessor::releaseResources() {}

bool MiditinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MiditinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
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
        if (std::exchange (pendingOffs[(size_t) m], false))
            midi.addEvent (juce::MidiMessage::noteOff (1, m), 0);

    fifo.read (fifo.getNumReady()).forEach ([&] (int i) {
        const auto& e = events[(size_t) i];
        switch (e.type)
        {
            case UiEvent::noteOn:      input.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), 0); break;
            case UiEvent::noteOff:     input.addEvent (juce::MidiMessage::noteOff (1, e.note), 0); break;
            case UiEvent::releaseHeld: engine.releaseHeld (midi, 0); break;
            case UiEvent::kick:        engine.kick(); break;
        }
    });

    engine.process (audioPatch, pv, tr, input, midi, n);
    if (audioPatch.preview)
        preview.render (midi, buffer);
    publishSnapshot();
}

void MiditinProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    if (! std::exchange (bypassed, true))
        engine.releaseAll (midi, 0);
}

void MiditinProcessor::publishSnapshot()
{
    engine.fillSnapshot (snapshots.back());
    snapshots.publish();
}

juce::AudioProcessorEditor* MiditinProcessor::createEditor() { return new MiditinEditor (*this); }

void MiditinProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("Miditin");
    if (auto params = apvts.copyState().createXml())
        root.addChildElement (params.release());
    root.addChildElement (new juce::XmlElement (patch.toXml()));
    copyXmlToBinary (root, dest);
}

void MiditinProcessor::setStateInformation (const void* data, int size)
{
    const auto root = getXmlFromBinary (data, size);
    if (root == nullptr || ! root->hasTagName ("Miditin"))
        return;
    if (auto* params = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));
    if (auto* p = root->getChildByName ("Patch"))
        setPatch (Patch::fromXml (*p));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MiditinProcessor(); }
