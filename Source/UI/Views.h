#pragma once
#include <juce_graphics/juce_graphics.h>
#include "Engine/Engine.h"

/** The animations: ports of the prototype's `draw()`, one function per canvas. Each draws into
    a 204 x 120 box whose origin is the canvas's top-left (keyboards: 429 x 44), from a `Frame` --
    the latest snapshot plus what the editor keeps between frames. Nothing here decides anything. */
namespace mdt::views
{
inline constexpr float CW = 204, CH = 120, KW = 429, KH = 44;
inline constexpr float rateArc = 0.55f;   // the energy rate arc's half-angle
inline constexpr int historyLength = 220;

/** An energy line, oldest first, for one view (voice). */
struct History
{
    int note = -1;
    std::array<float, historyLength> val {}, raw {};
    int count = 0;

    void push (float v, float r)
    {
        if (count == historyLength)
        {
            std::move (val.begin() + 1, val.end(), val.begin());
            std::move (raw.begin() + 1, raw.end(), raw.begin());
            --count;
        }
        val[(size_t) count] = v;
        raw[(size_t) count] = r;
        ++count;
    }
};

struct Frame
{
    const UiSnapshot* s = nullptr;
    const Patch* patch = nullptr;
    ParamValues pv;
    const std::array<History, maxVoices>* histories = nullptr;
    int numHistories = 0;
    double flow = 0;             // energy cable dash offset
    juce::String energyHover;    // "rate", "top", "bot", "mid" or empty

    double t() const { return s->t; }
    /** 1 at `t0`, fading to 0 over `len` seconds -- the prototype's `dk()`. */
    float dk (double t0, double len) const { return (float) std::clamp (1.0 - (s->t - t0) / len, 0.0, 1.0); }
};

/** Points the connections attach to, in each canvas's local coordinates. */
struct Anchors
{
    juce::Point<float> lfo, pTop, pOut, gC, gOut, tC;
};

/** Energy canvas geometry, shared with the editor's hit-testing. */
struct EnergyGeo
{
    float cx = CW / 2, cy = CH / 2 + 4, rm = CH / 2 - 11;
    float y (float v) const { return cy + rm - 6 - (2 * rm - 12) * v; }
    float v (float y) const { return juce::jlimit (0.0f, 1.0f, (cy + rm - 6 - y) / (2 * rm - 12)); }
    /** 0..1 position of the rate on its arc: log Hz, or the sync division. */
    static float rateN (const Patch&, const ParamValues&);
};

/** The transform wheel's geometry: the pitch range it shows and its spiral radii. */
struct WheelGeo
{
    int lo, hi;
    float cx, cy, r0, r1, rl, span;
    static WheelGeo make (const Patch&, const ParamValues&, const UiSnapshot&);
    float angle (int m) const;
    float radius (int m) const;
    juce::Point<float> at (int m) const;
};

/** The transformer's pool as the UI sees it, built the engine's way. */
Pool poolFor (const Patch&, const ParamValues&, const UiSnapshot&);

/** Where the connections attach, each in its own canvas's coordinates. */
Anchors anchorsFor (const Frame&);

void drawEnergy    (juce::Graphics&, const Frame&);
void drawPulse     (juce::Graphics&, const Frame&);
void drawGenerate  (juce::Graphics&, const Frame&);
void drawTransform (juce::Graphics&, const Frame&);
void drawInKeys    (juce::Graphics&, const Frame&);
void drawOutKeys   (juce::Graphics&, const Frame&);

/** `under` is everything drawn beneath the canvases; the energy cable rides over the wheel and the
    shaker but under the digital grid, so it is drawn in whichever pass that puts it. Anchors here
    are in the shared window coordinates. */
void drawConnections (juce::Graphics&, const Frame&, const Anchors& grid, bool under);

struct Key { int m; float x, w; bool black; };
std::vector<Key> keys (int lo, int hi, float width);

juce::String noteName (int m);
juce::String signedTone (int d);
} // namespace mdt::views
