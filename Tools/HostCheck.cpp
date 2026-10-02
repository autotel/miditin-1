#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

/** Loads the built VST3 the way a DAW does -- through JUCE's VST3 hosting, so the plugin's own
    wrapper, bus layout, MIDI output path and bypass handling are all in play -- and checks what
    the in-REAPER test would: MIDI comes out, the transport drives it, a stop leaves nothing
    hanging, state survives a reload into a fresh instance, and bypass releases what was sounding.

        ./build.sh hostcheck

    Exit code 0 = all good. No window is opened. */
namespace
{
struct Playhead final : juce::AudioPlayHead
{
    bool playing = true;
    double ppq = 0, bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        return p;
    }
};

int failures = 0;
void check (bool ok, const char* what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    failures += ! ok;
}

struct Run
{
    int ons = 0, offs = 0;
    std::array<int, 128> open {};
    bool doubled = false;

    void take (const juce::MidiBuffer& m)
    {
        for (const auto md : m)
        {
            const auto msg = md.getMessage();
            if (msg.isNoteOn()) { ++ons; doubled |= open[(size_t) msg.getNoteNumber()]++ > 0; }
            else if (msg.isNoteOff()) { ++offs; open[(size_t) msg.getNoteNumber()] = 0; }
        }
    }
    int sounding() const { int n = 0; for (int c : open) n += c > 0; return n; }
};

void render (juce::AudioPluginInstance& p, Playhead& ph, Run& run, double seconds, int block = 512)
{
    juce::AudioBuffer<float> audio (2, block);
    juce::MidiBuffer midi;
    for (double t = 0; t < seconds; t += block / 48000.0)
    {
        midi.clear();
        p.processBlock (audio, midi);
        run.take (midi);
        if (ph.playing) ph.ppq += block / 48000.0 * ph.bpm / 60.0;
    }
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::String path = argc > 1 ? argv[1] : "build/Miditin_artefacts/RelWithDebInfo/VST3/miditin 1.vst3";

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::File::getCurrentWorkingDirectory().getChildFile (path).getFullPathName());
    if (found.isEmpty())
    {
        std::cerr << "no VST3 at " << path << "\n";
        return 2;
    }

    auto make = [&] {
        juce::String err;
        auto p = format.createInstanceFromDescription (*found[0], 48000, 512, err);
        if (p == nullptr) { std::cerr << err << "\n"; std::exit (2); }
        p->enableAllBuses();
        p->prepareToPlay (48000, 512);
        return p;
    };

    std::cout << "loading " << found[0]->name << " (" << found[0]->manufacturerName << ")\n";
    auto a = make();
    Playhead ph;
    a->setPlayHead (&ph);

    check (a->producesMidi(), "declares MIDI output");

    Run run;
    render (*a, ph, run, 4);
    check (run.ons > 4, "plays notes while the transport runs");
    check (! run.doubled, "never two note-ons without a note-off between");

    ph.playing = false;
    render (*a, ph, run, 0.02);
    check (run.sounding() == 0, "transport stop leaves nothing sounding");

    // State: tweak through the host's own parameter API, save, load into a fresh instance.
    for (auto* prm : a->getParameters())
        if (prm->getName (64) == "Wheel slices") prm->setValueNotifyingHost (0.0f);
    juce::MemoryBlock state;
    a->getStateInformation (state);
    auto b = make();
    b->setStateInformation (state.getData(), (int) state.getSize());
    // Byte equality is too strict for the first trip -- a log-curve parameter's float goes through
    // its normalised value and back -- so: every parameter within rounding, and a second trip exact.
    bool same = a->getParameters().size() == b->getParameters().size();
    for (int i = 0; same && i < a->getParameters().size(); ++i)
        same = std::abs (a->getParameters()[i]->getValue() - b->getParameters()[i]->getValue()) < 1e-4f;
    check (same, "every parameter survives a reload into a fresh instance");
    juce::MemoryBlock once, twice;
    b->getStateInformation (once);
    auto c = make();
    c->setStateInformation (once.getData(), (int) once.getSize());
    c->getStateInformation (twice);
    check (once == twice, "a reloaded state saves back byte for byte");
    c->releaseResources();
    bool restored = false;
    for (auto* prm : b->getParameters())
        if (prm->getName (64) == "Wheel slices") restored = prm->getValue() < 0.01f;
    check (restored, "a changed parameter is restored");

    // Bypass mid-note: whatever was sounding is released.
    ph.playing = true;
    Run held;
    for (int i = 0; i < 200 && held.sounding() == 0; ++i)
        render (*a, ph, held, 0.0107);
    check (held.sounding() > 0, "a note is sounding before bypass");
    if (auto* bypass = a->getBypassParameter())
    {
        bypass->setValueNotifyingHost (1.0f);
        render (*a, ph, held, 0.05);
        check (held.sounding() == 0, "bypass releases every sounding note");
        bypass->setValueNotifyingHost (0.0f);
    }
    else
    {
        check (false, "host sees a bypass parameter");
    }

    a->releaseResources();
    b->releaseResources();
    std::cout << (failures ? "FAILED\n" : "all good\n");
    return failures ? 1 : 0;
}
