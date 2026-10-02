#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Controls.h"
#include "Views.h"

/** The whole 900 x 600 window, laid out as the v2 prototype: a header, four modules left to right
    (ENERGY, PULSE, GENERATE, TRANSFORM) with their animations and controls, the IN and OUT
    keyboards along the bottom, and the connections drawn between the animations.

    It paints every animation itself from the host's latest snapshot, at the frame rate, and owns
    the controls as children. It never decides when a note happens. */
namespace mdt
{
class Shell : public juce::Component, private juce::Timer
{
public:
    static constexpr int width = 900, height = 600;

    explicit Shell (Host&);
    ~Shell() override;

    /** The librarian, shown over everything below the header while PRESETS is on. Borrowed. */
    void setPresetPanel (juce::Component*);
    void setPresetName (const juce::String&);
    /** The patch changed behind the shell's back (a preset load): re-read it. */
    void patchReloaded();

    /** One frame without the timer: the snapshot tool calls this before painting. */
    void advanceFrame();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // Geometry, in Shell coordinates. Exposed for the snapshot tool and hit-testing.
    static constexpr int headerH = 45, gridTop = headerH, bottomTop = 440, colW = 225;
    static juce::Rectangle<float> canvas (int column);
    static juce::Rectangle<float> keyboard (int side);   // 0 = IN, 1 = OUT

private:
    Host& host;
    Patch patch;                 // the editor's copy; every edit goes through `edit()`
    uint32_t seenVersion = 0;
    const UiSnapshot* snap = nullptr;
    uint64_t lastSerial = 0;
    double lastT = 0, flow = 0;
    std::array<views::History, maxVoices> histories {};
    int numHistories = 0;
    juce::String energyHover;

    struct Drag
    {
        enum Mode { none, rate, mid, top, bot, melDraw, melResize } mode = none;
        float x0 = 0, n0 = 0, v0 = 0, base0 = 0;
        int s = 0, c = 0;
    } drag;

    // header
    BoxButton runBtn { "", fonts::headerButton }, voiceBtn { "MONO", fonts::headerButton }, soundBtn { "", fonts::headerButton },
              presetsBtn { "PRESETS", fonts::headerButton };
    juce::Component* presetPanel = nullptr;
    std::unique_ptr<RangeRow> bpm;
    // energy
    Segmented enSource { { "OSC", "VEL", "NOTE", "CC", "AUDIO" }, 5, fonts::buttonSmall };
    Segmented enShape { { "SIN", "TRI", "SAW", "SQR", "S&H" }, 5 };
    BoxButton syncBtn { "SYNC" }, retrigBtn { "RETRIG" };
    std::unique_ptr<RangeRow> ccNumber, enAmount;
    Segmented enFilter { { "OFF", "LPF", "HPF" }, 3 };
    Segmented enEdge { { "BOTH", "+ ONLY", "- ONLY" }, 3, fonts::buttonSmall };
    // pulse
    Segmented pulseKind { { "WHEEL", "SHAKER", "DIGITAL", "MIDI IN" }, 2 };
    std::unique_ptr<RangeRow> slices, speed, agitate, balls, bounce, digDiv, swing;
    std::array<std::unique_ptr<RangeRow>, 3> modOffset, modScale;
    BoxButton kickBtn { "" };
    Segmented digBase { { "2", "3", "5", "7" }, 4 };
    Segmented shakeMode { { "STRENGTH", "POSITION" }, 2, fonts::buttonSmall };   // what energy is to the box
    Segmented modMode { { "SCALE", "OFFSET" }, 2 };
    // generate
    Segmented genKind { { "ARP", "INPUT", "MELODY" }, 3 };
    juce::TextEditor tones;
    Segmented order { { "UP", "DN", "UP/DN", "RND", "TYPED" }, 5, fonts::buttonSmall };
    std::unique_ptr<RangeRow> melLength;
    BoxButton transposeBtn { "TRANSPOSE BY IN" }, clearBtn { "CLEAR" };
    Segmented runMode { { "FREE RUN", "RESET ON IN" }, 2, fonts::buttonSmall };
    // transform
    Segmented tfMode { { "SCALE", "SET", "HELD" }, 3 };
    std::unique_ptr<RangeRow> octLen, octave;
    BoxButton wrapBtn { "" };
    Segmented scale { { "major", "minor", "dorian", "phryg", "pent+", "pent-", "whole", "chrom" }, 4, fonts::buttonSmall };
    // in / out
    BoxButton releaseBtn { "RELEASE ALL" };
    std::unique_ptr<RangeRow> gate;
    std::array<std::unique_ptr<RangeRow>, 3> vel;   // VELOCITY: energy, pulse, input

    /** Labels drawn beside controls ("FILTER", "BASE"...), placed by `layoutControls()`. */
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> sideLabels;
    juce::Rectangle<float> heldTextArea, inHeldArea, logArea, velArea;

    void timerCallback() override;
    void edit (const std::function<void (Patch&)>&);
    void syncControls();
    void layoutControls();
    views::Frame frame() const;
    void drawModuleTitles (juce::Graphics&);
    void drawBottomText (juce::Graphics&);

    juce::String energyZone (juce::Point<float> local) const;
};
} // namespace mdt
