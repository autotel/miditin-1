#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <PresetPanel.h>
#include "Model/Document.h"
#include "UI/Shell.h"
#include "UI/Theme.h"

/** Renders the editor to PNGs with no window: a headless engine runs a few seconds of a scene,
    then the shell paints its latest snapshot into an image. The dev machine is a Wayland session
    and an agent must never open a window on the user's display.

        ./build/MiditinSnap_artefacts/RelWithDebInfo/MiditinSnap <output-dir>

    Compare against the prototype (`Miditin 1 plugin design/miditin 1 v2.dc.html`). */
namespace
{
using namespace mdt;

struct FakeHost final : Host
{
    Engine engine;
    Patch p;
    ParamValues pv = ParamValues::defaults();
    UiSnapshot snap;
    uint32_t version = 0;
    std::vector<int> pressed;
    std::vector<int> ccs;   // controller numbers moved in the first block
    bool learn = false;     // press LEARN
    double wall = 0;   // simulated display time, one frame per block

    FakeHost() { engine.prepare (48000); }

    const Patch& patch() const override { return p; }
    void setPatch (const Patch& x) override { p = x; ++version; }
    uint32_t patchVersion() const override { return version; }
    float param (P k) const override { return pv[k]; }
    void setParam (P k, float v) override { pv[k] = v; }
    const UiSnapshot& snapshot() override { return snap; }
    void noteOn (int n, float) override { pressed.push_back (n); }
    void noteOff (int) override {}
    void releaseHeld() override {}
    void kick() override {}

    /** Runs the engine, stopping `frames` times to let the shell take a frame (energy history). */
    void run (double seconds, Shell& shell, std::vector<int> notes = {})
    {
        juce::MidiBuffer in, out;
        for (int n : notes) in.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        for (int n : ccs) in.addEvent (juce::MidiMessage::controllerEvent (1, n, 64), 0);
        const int block = 800;   // 60 frames a second at 48 kHz
        for (double t = 0; t < seconds; t += block / 48000.0)
        {
            out.clear();
            engine.process (p, pv, {}, in, out, block);
            in.clear();
            engine.fillSnapshot (snap);
            shell.advanceFrame (wall += block / 48000.0);
        }
    }
};

/** The librarian against a scratch folder, so the sheet shows the built-ins and nothing personal. */
struct FakePresets final : atl::PresetHost
{
    juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("miditin-snap-presets");
    juce::ValueTree captureDocument() override { return makeDocument ({}, ParamValues::defaults()); }
    void applyDocument (const juce::ValueTree&) override {}
    juce::File presetFolder() const override { return folder; }
    void setPresetFolder (const juce::File&) override {}
    bool documentHasUnsavedChanges() const override { return false; }
};

void render (const juce::File& dir, const juce::String& name, const std::function<void (FakeHost&)>& setup,
             std::vector<int> notes = {}, double seconds = 4, bool presets = false)
{
    FakeHost host;
    setup (host);
    Shell shell (host);
    FakePresets presetHost;
    const atl::PresetFormat format { ".miditin", documentType };
    atl::PresetPanel panel (presetHost, format);
    std::vector<juce::MemoryBlock> data;
    std::vector<atl::BuiltInPreset> builtIns;
    data.reserve (factoryPresets().size());
    for (const auto& f : factoryPresets())
    {
        juce::MemoryOutputStream out;
        makeDocument (f.patch, f.pv).writeToStream (out);
        data.push_back (out.getMemoryBlock());
        builtIns.push_back ({ f.name, f.category, data.back().getData(), (int) data.back().getSize() });
    }
    atl::PresetPanelColours c;
    c.background = theme::paper; c.panel = theme::shade; c.text = theme::ink; c.textDim = theme::pillText;
    c.accent = theme::accent; c.outline = theme::line2;
    panel.setColours (c);
    panel.setBuiltIns (builtIns);
    shell.setPresetPanel (&panel);
    shell.setPresetName ("Wheel arpeggio");
    panel.setVisible (presets);
    if (auto* b = dynamic_cast<BoxButton*> (shell.findChildWithID ("learn")); b != nullptr && host.learn)
        b->onClick();
    host.run (seconds, shell, notes);
    const int scale = 2;
    juce::Image img (juce::Image::ARGB, Shell::width * scale, Shell::height * scale, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) scale));
        shell.paintEntireComponent (g, true);
    }
    juce::FileOutputStream os (dir.getChildFile (name + ".png"));
    os.setPosition (0);
    os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << dir.getChildFile (name + ".png").getFullPathName() << "\n";
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    render (dir, "default", [] (FakeHost&) {}, { 57, 60, 64 });
    render (dir, "presets", [] (FakeHost&) {}, {}, 0.5, true);
    render (dir, "shaker-poly-melody", [] (FakeHost& h) {
        h.p.pulse = PulseKind::shaker;
        h.p.voiceMode = VoiceMode::poly;
        h.p.gen = GenKind::melody;
        h.p.tfMode = TransformMode::scale;
        h.p.enFilter = EnergyFilter::lpf;
        h.p.enEdge = EnergyEdge::neg;
    }, { 57, 64 });
    render (dir, "digital-input-set", [] (FakeHost& h) {
        h.p.pulse = PulseKind::digital;
        h.p.gen = GenKind::input;
        h.p.tfMode = TransformMode::set;
        h.p.enSync = true;
        h.p.enRetrig = true;
        h.p.enFilter = EnergyFilter::hpf;
        h.pv[P::enHpfZero] = 0.7f;
        h.pv[P::tfOctave] = 4.25f;
    }, { 60, 64, 67 });
    render (dir, "midiin-held-nowrap", [] (FakeHost& h) {
        h.p.pulse = PulseKind::midiIn;
        h.p.tfMode = TransformMode::held;
        h.p.wrap = false;
        h.p.enSource = EnergySource::cc;
    }, { 48, 55, 62 });
    render (dir, "midiin-clock", [] (FakeHost& h) {
        h.p.pulse = PulseKind::midiIn;
        h.p.midiInSource = MidiInSource::clock;
        h.p.clockDiv = 4;
        h.p.clockStart = ClockStart::note;   // shows START and LEGATO
    }, { 57, 60, 64 });
    render (dir, "cc-learn", [] (FakeHost& h) {
        h.p.enSource = EnergySource::cc;
        h.p.ccNumber = 74;
        h.ccs = { 1, 74, 11 };
        h.learn = true;
    }, { 60, 64, 67 });
    render (dir, "audio-gain", [] (FakeHost& h) {
        h.p.enSource = EnergySource::audio;
        h.pv[P::enGain] = 12;
    }, { 60, 64, 67 });
    return 0;
}
