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
        ./build/MiditinSnap_artefacts/RelWithDebInfo/MiditinSnap --manual docs/images

    Compare against the prototype (`Miditin 1 plugin design/miditin 1 v2.dc.html`). `--manual`
    renders the illustrations of `docs/MANUAL.md` instead: cropped, with lettered rings. */
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

    /** Runs the engine, stopping every block to let the shell take a frame (energy history).
        `later` presses (positive) or lifts (negative) a note at a time in seconds. */
    void run (double seconds, Shell& shell, std::vector<int> notes = {}, std::vector<std::pair<double, int>> later = {})
    {
        juce::MidiBuffer in, out;
        for (int n : notes) in.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        for (int n : ccs)
            in.addEvent (n == ccPolyAt ? juce::MidiMessage::aftertouchChange (1, 60, 64) : juce::MidiMessage::controllerEvent (1, n, 64), 0);
        const int block = 800;   // 60 frames a second at 48 kHz
        for (double t = 0; t < seconds; t += block / 48000.0)
        {
            for (const auto& [at, n] : later)
                if (at >= t && at < t + block / 48000.0)
                    in.addEvent (n > 0 ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, -n), 0);
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

/** A lettered ring drawn over the picture, in Shell coordinates: the manual's callouts. The badge
    goes beside a short row, so it hides no label, and on the corner of a large area. */
struct Mark
{
    enum class Place { automatic, corner, left, right, above };
    juce::Rectangle<float> r;
    juce::String tag;
    Place place = Place::automatic;
};

struct Shot
{
    juce::String name;
    std::function<void (FakeHost&)> setup = [] (FakeHost&) {};
    std::vector<int> notes;
    double seconds = 4;
    bool presets = false;
    juce::String presetName = "Wheel arpeggio";
    std::vector<std::pair<double, int>> later;
    juce::Rectangle<int> crop { 0, 0, Shell::width, Shell::height };
    std::vector<Mark> marks;
};

const juce::Colour markColour { 0xff7b4dff };   // in no part of the palette, so never mistaken for the UI

void drawMark (juce::Graphics& g, const Mark& m)
{
    g.setColour (markColour);
    g.drawRoundedRectangle (m.r, 4, 1.6f);
    if (m.tag.isEmpty())
        return;
    const auto font = fonts::get (fonts::Family::mono, 600, 10);
    const float w = std::max (15.0f, juce::GlyphArrangement::getStringWidth (font, m.tag) + 8);
    auto place = m.place == Mark::Place::automatic ? (m.r.getHeight() < 40 ? Mark::Place::left : Mark::Place::corner) : m.place;
    const auto cy = m.r.getCentreY() - 7.5f;
    const juce::Rectangle<float> badge = place == Mark::Place::left  ? juce::Rectangle<float> (m.r.getX() - w - 2, cy, w, 15)
                                       : place == Mark::Place::right ? juce::Rectangle<float> (m.r.getRight() + 2, cy, w, 15)
                                       : place == Mark::Place::above ? juce::Rectangle<float> (m.r.getX(), m.r.getY() - 16, w, 15)
                                                                     : juce::Rectangle<float> (m.r.getX() - w / 2 + 2, m.r.getY() - 7.5f + 2, w, 15);
    g.fillRoundedRectangle (badge, 7.5f);
    g.setColour (juce::Colours::white);
    g.setFont (font);
    g.drawText (m.tag, badge, juce::Justification::centred);
}

void render (const juce::File& dir, const Shot& shot)
{
    FakeHost host;
    shot.setup (host);
    Shell shell (host);
    FakePresets presetHost;
    const atl::PresetFormat format { ".miditin", documentType };
    atl::PresetPanel panel (presetHost, format);
    std::vector<juce::MemoryBlock> data;
    std::vector<atl::BuiltInPreset> builtIns;
    data.reserve (factoryPresets().size());
    for (const auto& f : factoryPresets())
    {
        data.push_back (factoryPresetData (f, format));
        builtIns.push_back ({ f.name, f.category, data.back().getData(), (int) data.back().getSize() });
    }
    atl::PresetPanelColours c;
    c.background = theme::paper; c.panel = theme::shade; c.text = theme::ink; c.textDim = theme::pillText;
    c.accent = theme::accent; c.outline = theme::line2;
    panel.setColours (c);
    panel.setBuiltIns (builtIns);
    shell.setPresetPanel (&panel);
    shell.setPresetName (shot.presetName);
    panel.setVisible (shot.presets);
    if (auto* b = dynamic_cast<BoxButton*> (shell.findChildWithID ("learn")); b != nullptr && host.learn)
        b->onClick();
    host.run (shot.seconds, shell, shot.notes, shot.later);

    // Callouts need room past the picture's edge for their badges.
    const int scale = 2, margin = shot.marks.empty() ? 0 : 12;
    const auto crop = shot.crop;
    juce::Image img (juce::Image::ARGB, (crop.getWidth() + 2 * margin) * scale, (crop.getHeight() + 2 * margin) * scale, true);
    {
        juce::Graphics g (img);
        g.fillAll (theme::ground);
        g.addTransform (juce::AffineTransform::translation ((float) (margin - crop.getX()), (float) (margin - crop.getY()))
                            .scaled ((float) scale));
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (crop);
            shell.paintEntireComponent (g, true);
        }
        for (const auto& m : shot.marks)
            drawMark (g, m);
    }
    juce::FileOutputStream os (dir.getChildFile (shot.name + ".png"));
    os.setPosition (0);
    os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << dir.getChildFile (shot.name + ".png").getFullPathName() << "\n";
}

void render (const juce::File& dir, const juce::String& name, const std::function<void (FakeHost&)>& setup,
             std::vector<int> notes = {}, double seconds = 4, bool presets = false)
{
    Shot s;
    s.name = name;
    s.setup = setup;
    s.notes = std::move (notes);
    s.seconds = seconds;
    s.presets = presets;
    render (dir, s);
}

/** The illustrations of docs/MANUAL.md. `crop` and `marks` are Shell coordinates read off a render
    of each scene: a layout change moves them, so re-render and look before committing. */
std::vector<Shot> manualShots()
{
    const std::vector<int> chord { 57, 60, 64 }, triad { 60, 64, 67 };
    const juce::Rectangle<int> whole { 0, 0, Shell::width, Shell::height };
    auto column = [] (int i, int h) { return juce::Rectangle<int> (i * Shell::colW, 50, Shell::colW, h); };
    std::vector<Shot> v;
    v.reserve (32);
    auto shot = [&v] (const char* name, std::vector<int> notes, juce::Rectangle<int> crop) -> Shot& {
        auto& s = v.emplace_back();
        s.name = name;
        s.notes = std::move (notes);
        s.crop = crop;
        s.presetName = {};
        return s;
    };
    // Rows that recur: the readout under each animation, and a column's full-width control rows.
    const juce::Rectangle<float> rateArc { 78, 96, 102, 20 }, wheel { 272, 98, 106, 104 },
                                 inKeys { 8, 486, 434, 48 }, outKeys { 458, 486, 434, 48 };

    {
        auto& s = shot ("quickstart", chord, whole);
        s.presetName = "Wheel arpeggio";
        s.marks = { { inKeys, "1" }, { wheel, "2" }, { { 505, 92, 115, 102 }, "3" }, { { 727, 92, 122, 122 }, "4" }, { outKeys, "5" } };
    }
    {
        auto& s = shot ("overview", chord, whole);
        s.presetName = "Wheel arpeggio";
        s.marks = { { { 3, 3, 894, 39 }, "A" }, { { 4, 52, 217, 300 }, "B" }, { { 229, 52, 217, 355 }, "C" },
                    { { 454, 52, 217, 262 }, "D" }, { { 679, 52, 217, 252 }, "E" }, { { 454, 388, 442, 50 }, "F" },
                    { { 4, 448, 442, 119 }, "G" }, { { 454, 448, 442, 119 }, "H" } };
    }
    {
        auto& s = shot ("header", chord, { 0, 0, Shell::width, Shell::headerH });
        s.presetName = "Wheel arpeggio";
        s.marks = { { { 12, 10, 82, 25 }, "1" }, { { 98, 14, 262, 17 }, "2" }, { { 381, 8, 126, 29 }, "3" },
                    { { 511, 12, 112, 21 }, "4" }, { { 631, 12, 92, 21 }, "5" }, { { 735, 8, 58, 29 }, "6" },
                    { { 797, 8, 90, 29 }, "7" } };
        for (auto& m : s.marks)
            m.place = Mark::Place::above;   // the header is one row: beside would hide the neighbour
    }

    // ENERGY
    {
        auto& s = shot ("energy-osc", triad, column (0, 300));
        s.setup = [] (FakeHost& h) { h.p.enRetrig = true; };
        s.marks = { { rateArc, "1" }, { { 60, 122, 104, 70 }, "2" }, { { 154, 121, 13, 13 }, "3" }, { { 6, 202, 30, 13 }, "4" },
                    { { 8, 222, 208, 26 }, "5" }, { { 8, 252, 208, 25 }, "6" }, { { 8, 279, 208, 22 }, "7" },
                    { { 8, 302, 208, 18 }, "8" }, { { 8, 322, 208, 25 }, "9" } };
    }
    {
        auto& s = shot ("energy-lpf", triad, column (0, 330));
        s.setup = [] (FakeHost& h) {
            h.p.enShape = Shape::square;
            h.p.enSync = true;
            h.pv[P::enDiv] = 3;
            h.p.enFilter = EnergyFilter::lpf;
            h.pv[P::enFilterAmt] = 0.4f;
        };
        s.marks = { { { 78, 94, 86, 22 }, "1" }, { { 64, 124, 92, 66 }, "2" }, { { 190, 202, 28, 13 }, "3" },
                    { { 8, 331, 208, 26 }, "4" }, { { 8, 358, 208, 16 }, "5" } };
    }
    {
        auto& s = shot ("energy-hpf", triad, column (0, 330));
        s.setup = [] (FakeHost& h) {
            h.pv[P::enRate] = 0.4f;
            h.p.enFilter = EnergyFilter::hpf;
            h.p.enEdge = EnergyEdge::pos;
            h.pv[P::enHpfZero] = 0.25f;
        };
        s.marks = { { { 62, 173, 102, 11 }, "1" }, { { 190, 202, 28, 13 }, "2" }, { { 8, 331, 208, 26 }, "3" }, { { 8, 358, 208, 16 }, "4" } };
    }
    {
        auto& s = shot ("energy-cc", triad, column (0, 300));
        s.setup = [] (FakeHost& h) {
            h.p.enSource = EnergySource::cc;
            h.p.ccNumber = 74;
            h.ccs = { 1, 74, ccPolyAt };
            h.learn = true;
        };
        s.marks = { { { 8, 248, 208, 22 }, "1" }, { { 8, 269, 154, 22 }, "2" }, { { 190, 202, 28, 13 }, "3" } };
    }
    {
        auto& s = shot ("energy-audio", triad, column (0, 300));
        s.setup = [] (FakeHost& h) {
            h.p.enSource = EnergySource::audio;
            h.pv[P::enGain] = 12;
        };
        s.marks = { { rateArc, "1" }, { { 186, 202, 32, 13 }, "2" } };
    }

    // PULSE
    {
        auto& s = shot ("pulse-wheel", triad, column (1, 360));
        s.marks = { { wheel, "1" }, { { 368, 142, 24, 18 }, "2", Mark::Place::right }, { { 232, 203, 54, 13 }, "3" }, { { 402, 203, 40, 13 }, "4" },
                    { { 233, 274, 208, 32 }, "5" }, { { 233, 309, 208, 28 }, "6" }, { { 233, 343, 208, 26 }, "7" },
                    { { 233, 372, 208, 33 }, "8" } };
    }
    {
        auto& s = shot ("pulse-shaker", triad, column (1, 375));
        s.setup = [] (FakeHost& h) { h.p.pulse = PulseKind::shaker; };
        s.marks = { { { 288, 183, 44, 36 }, "1" }, { { 280, 98, 26, 13 }, "2" }, { { 233, 274, 208, 51 }, "3", Mark::Place::left },
                    { { 233, 327, 208, 28 }, "4" }, { { 233, 361, 208, 26 }, "5" } };
    }
    {
        auto& s = shot ("pulse-digital", triad, column (1, 330));
        s.setup = [] (FakeHost& h) {
            h.p.pulse = PulseKind::digital;
            h.pv[P::dgDiv] = 3;
        };
        s.marks = { { { 233, 169, 208, 25 }, "1" }, { { 405, 102, 14, 114 }, "2" }, { { 232, 91, 60, 14 }, "3" },
                    { { 233, 273, 208, 26 }, "4" }, { { 233, 300, 208, 35 }, "5" } };
    }
    {
        auto& s = shot ("pulse-clock", triad, column (1, 330));
        s.setup = [] (FakeHost& h) {
            h.p.pulse = PulseKind::midiIn;
            h.p.midiInSource = MidiInSource::clock;
            h.p.clockStart = ClockStart::note;
        };
        s.marks = { { { 307, 127, 62, 62 }, "1" }, { { 233, 274, 208, 26 }, "2" }, { { 233, 301, 208, 46 }, "3", Mark::Place::left }, { { 233, 349, 208, 26 }, "4" } };
    }

    // GENERATE
    {
        auto& s = shot ("generate-arp", triad, column (2, 290));
        s.setup = [] (FakeHost& h) {
            h.p.gen = GenKind::arp;
            h.p.order = Order::upDown;
            h.p.tfMode = TransformMode::scale;
        };
        s.marks = { { { 502, 95, 122, 120 }, "1" }, { { 622, 91, 46, 14 }, "2", Mark::Place::above }, { { 458, 251, 208, 26 }, "3" },
                    { { 458, 279, 208, 28 }, "4" }, { { 458, 309, 208, 26 }, "5" } };
    }
    {
        auto& s = shot ("generate-input", { 57, 60, 64, 69 }, column (2, 270));
        s.marks = { { { 500, 95, 118, 120 }, "1" }, { { 458, 251, 208, 27 }, "2" }, { { 458, 282, 208, 26 }, "3" } };
    }
    {
        auto& s = shot ("generate-melody", triad, column (2, 280));
        s.setup = [] (FakeHost& h) {
            h.p.gen = GenKind::melody;
            h.p.tfMode = TransformMode::scale;
        };
        s.marks = { { { 649, 104, 18, 111 }, "1" }, { { 509, 101, 44, 17 }, "2", Mark::Place::right },
                    { { 457, 91, 66, 14 }, "3" }, { { 458, 250, 208, 18 }, "4" }, { { 510, 269, 156, 22 }, "5" } };
    }

    // TRANSFORM
    {
        auto& s = shot ("transform-scale", triad, column (3, 300));
        s.setup = [] (FakeHost& h) {
            h.p.gen = GenKind::arp;
            h.p.tfMode = TransformMode::scale;
        };
        s.marks = { { { 724, 147, 19, 19 }, "1" }, { { 682, 91, 40, 14 }, "2" }, { { 862, 91, 30, 14 }, "3" },
                    { { 683, 251, 208, 22 }, "4" }, { { 683, 275, 208, 46 }, "5", Mark::Place::left }, { { 683, 323, 208, 16 }, "6" } };
    }
    {
        auto& s = shot ("transform-set", triad, column (3, 300));
        s.setup = [] (FakeHost& h) {
            h.p.gen = GenKind::arp;
            h.p.tfMode = TransformMode::set;
            h.pv[P::tfOctave] = 4.25f;
        };
        s.marks = { { { 792, 146, 20, 20 }, "1" }, { { 779, 95, 16, 16 }, "2" }, { { 683, 250, 208, 18 }, "3" }, { { 683, 292, 208, 17 }, "4" } };
    }
    {
        auto& s = shot ("transform-held", { 48, 55, 62 }, column (3, 260));
        s.setup = [] (FakeHost& h) { h.p.wrap = false; };
        s.marks = { { { 746, 114, 82, 84 }, "1" }, { { 683, 251, 208, 22 }, "2" }, { { 683, 277, 50, 14 }, "3" } };
    }

    // IN / OUT, voices, presets
    {
        // A digital pulse: its controls end above the GATE row, so the crop shows only the bottom.
        auto& s = shot ("in-out", chord, { 0, 380, Shell::width, Shell::height - 380 });
        s.setup = [] (FakeHost& h) {
            h.p.enRetrig = true;
            h.p.pulse = PulseKind::digital;
        };
        s.marks = { { { 162, 465, 126, 15 }, "1" }, { inKeys, "2" }, { { 7, 541, 90, 15 }, "3" }, { { 270, 537, 84, 22 }, "4", Mark::Place::corner },
                    { { 358, 537, 84, 22 }, "5", Mark::Place::corner }, { { 458, 391, 145, 16 }, "6" }, { { 458, 408, 364, 30 }, "7" },
                    { outKeys, "8" }, { { 718, 537, 174, 21 }, "9" } };
    }
    {
        auto& s = shot ("poly", { 48 }, whole);
        s.setup = [] (FakeHost& h) {
            h.p.voiceMode = VoiceMode::poly;
            h.p.enRetrig = true;
            h.p.legato = false;
        };
        s.later = { { 0.9, 55 }, { 1.9, 64 } };
        s.marks = { { { 735, 8, 58, 29 }, "1", Mark::Place::above }, { { 7, 202, 46, 13 }, "2" }, { wheel, "3" },
                    { { 546, 138, 32, 33 }, "4", Mark::Place::right } };
    }
    {
        auto& s = shot ("presets", {}, whole);
        s.presetName = "Wheel arpeggio";
        s.seconds = 0.5;
        s.presets = true;
        s.marks = { { { 381, 8, 126, 29 }, "1", Mark::Place::above } };
    }
    return v;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File dir (argc > 1 && juce::String (argv[1]) != "--manual" ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    if (argc > 2 && juce::String (argv[1]) == "--manual")
    {
        const juce::File docs (argv[2]);
        docs.createDirectory();
        for (const auto& shot : manualShots())
            render (docs, shot);
        return 0;
    }

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
        h.ccs = { 1, 74, ccPolyAt };
        h.learn = true;
    }, { 60, 64, 67 });
    render (dir, "audio-gain", [] (FakeHost& h) {
        h.p.enSource = EnergySource::audio;
        h.pv[P::enGain] = 12;
    }, { 60, 64, 67 });
    return 0;
}
