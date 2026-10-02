#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Model/ParamRegistry.h"
#include "Model/Patch.h"
#include "Energy.h"
#include "Generate.h"
#include "Pulse.h"
#include "Transformer.h"

/** The whole chain, run on the audio thread at a fixed control rate (wiki/architecture.md).
    ENERGY drives PULSE, each pulse advances GENERATE, TRANSFORM maps the tone to a note, and the
    note table guarantees every note-on its note-off. A port of the prototype's `step()`,
    `pulse()`, `inputNote()` and friends, minus everything that draws. */
namespace mdt
{
struct Transport
{
    bool   known   = false;   // the host reports transport at all
    bool   playing = false;
    double bpm     = 0;       // 0 = host gave none
    double ppq     = 0;       // at the block's first sample
};

/** One energy -> pulse -> generate chain. Mono runs the engine's own; poly one per held note. */
struct Chain
{
    bool     active = false;
    int      note   = -1;     // poly: the input note that owns this chain
    uint32_t age    = 0;
    EnergyInputs in;
    double   lfoOff = 0;
    Lfo      lfo;
    float    energy = 0.5f;
    Wheel    wheel;
    Shaker   shaker;
    Digital  digital;
    double   lastPulseT = -9;
    GenPos   gen;
    int      melNote = -1;    // the melody note this chain may be holding through ties
    bool     hasDeg  = false;
    int      curDeg  = 0;
    Rng      rng;
};

inline constexpr int maxVoices = 16;
inline constexpr double holdForever = 1e300;

/** What the editor animates from. Published once per block, latest wins. */
struct UiSnapshot
{
    struct View
    {
        int    note = -1;
        float  energy = 0.5f, energyRaw = 0.5f;
        double wheelA = 0, wheelW = 0;
        float  flick = 0;
        int    ballCount = 0;
        std::array<Ball, Shaker::maxBalls> balls {};
        std::array<WallHit, 8> walls {};
        float  effort = 0;
        float  boxX = 0, boxY = 0;   // the shaker box's displacement, box widths
        double beat = 0, digT = -9;
        int    digDiv = 2;
        int    arpI = -1, arpPrev = -1, melI = -1;
    };
    struct Mapping { int deg = 0, note = -1; double t = -9; };
    struct Sounding { bool on = false; int vel = 0; double until = -9; };
    struct LogItem { int note = 0, deg = 0; };

    uint64_t serial = 0;
    double t = 0, clock = 0;
    float  energy = 0.5f;
    bool   poly = false;
    int    numViews = 1;
    std::array<View, maxVoices> views {};
    double pulseT = -9, genT = -9, inT = -9;
    float  pulseV = 0.5f;
    bool   hasDeg = false;
    int    curDeg = 0;
    std::array<Mapping, 16> mappings {};   // ring, newest at mapNext - 1
    int    mapNext = 0;
    std::array<Sounding, 128> notes {};
    std::array<int, 128> held {};          // press order
    int    heldCount = 0;
    ToneSeq seq;
    std::array<LogItem, 5> log {};          // newest first
    int    logCount = 0;
    float  cc = 0.5f;
};

/** The display between two snapshots: `b` with its continuous state (time, energy, wheel, balls,
    box, beat) taken back to engine time `t` in [a.t, b.t]. The audio thread publishes once per
    block, so frames between blocks would otherwise stall and jump. Discrete state is `b`'s. */
inline void blend (const UiSnapshot& a, const UiSnapshot& b, double t, UiSnapshot& out)
{
    out = b;
    if (b.t <= a.t || a.numViews != b.numViews)
        return;
    const double x = std::clamp ((t - a.t) / (b.t - a.t), 0.0, 1.0);
    const auto mix = [x] (auto p, auto q) { return (decltype (q)) (p + (q - p) * x); };
    out.t = mix (a.t, b.t);
    out.clock = mix (a.clock, b.clock);
    out.energy = mix (a.energy, b.energy);
    for (int i = 0; i < b.numViews; ++i)
    {
        const auto& p = a.views[(size_t) i];
        auto& q = out.views[(size_t) i];
        if (p.note != q.note)
            continue;
        q.energy = mix (p.energy, q.energy);
        q.energyRaw = mix (p.energyRaw, q.energyRaw);
        q.wheelA = mix (p.wheelA, q.wheelA);
        q.wheelW = mix (p.wheelW, q.wheelW);
        q.flick = mix (p.flick, q.flick);
        q.effort = mix (p.effort, q.effort);
        q.boxX = mix (p.boxX, q.boxX);
        q.boxY = mix (p.boxY, q.boxY);
        if (std::abs (q.beat - p.beat) < 1)   // not across a relock to the host bar
            q.beat = mix (p.beat, q.beat);
        if (p.ballCount == q.ballCount)
            for (int k = 0; k < q.ballCount; ++k)
            {
                q.balls[(size_t) k].x = mix (p.balls[(size_t) k].x, q.balls[(size_t) k].x);
                q.balls[(size_t) k].y = mix (p.balls[(size_t) k].y, q.balls[(size_t) k].y);
            }
    }
}

class Engine
{
public:
    static constexpr double tickRate    = 1000.0;
    static constexpr double dt          = 1.0 / tickRate;
    /** The prototype's `minPulseGap` default: a chain cannot pulse faster than this. */
    static constexpr double minPulseGap = 0.030;

    /** Resets everything, including the random seeds. */
    void prepare (double sampleRate);

    /** `in` and `out` must be different buffers. `out` receives only generated notes. `audio`
        (`audioChannels` channels of `numSamples`) feeds the AUDIO energy source; none = silence. */
    void process (const Patch&, const ParamValues&, const Transport&,
                  const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const float* const* audio = nullptr, int audioChannels = 0);

    /** Every sounding output note off, now. Bypass, `releaseResources`, transport stop. */
    void releaseAll (juce::MidiBuffer& out, int samplePos);

    /** The IN keyboard's RELEASE ALL: drops every held input note (and its poly voice). */
    void releaseHeld (juce::MidiBuffer& out, int samplePos);

    /** SPIN / SHAKE. */
    void kick();

    void fillSnapshot (UiSnapshot&) const;

    int heldCount() const noexcept { return numHeld; }
    int activeVoices() const noexcept;
    int sounding() const noexcept;

private:
    double sampleRate = 44100, samplesPerTick = 44.1, nextTick = 0;
    double t = 0, clock = 0, prevClock = 0, bpm = 120;
    bool   wasPlaying = false, primed = false;
    uint32_t ageCounter = 0;
    Rng    seeder;

    Patch       patch;
    ParamValues pv = ParamValues::defaults();
    EnergySettings energySettings;

    Chain global;
    std::array<Chain, maxVoices> voices {};

    std::array<int, 128> held {};
    std::array<float, 128> heldVel {};   // beside `held`: each held note's velocity
    int   numHeld = 0;
    float cc = 0.5f;
    // ponytail: fixed 30 ms RMS window; a knob if it proves too jumpy or too slow.
    static constexpr double rmsTau = 0.030;
    double meanSquare = 0, msCoef = 0;
    float  audioLevel = 0;
    ToneSeq polySeq, monoSeq;
    Pool  pool;

    struct Sounding { bool on = false; int vel = 0; double until = -9; };
    std::array<Sounding, 128> notes {};

    // display-only state
    double pulseT = -9, genT = -9, inT = -9;
    float  pulseV = 0.5f;
    std::array<UiSnapshot::Mapping, 16> mappings {};
    int    mapNext = 0;
    std::array<UiSnapshot::LogItem, 5> log {};
    int    logCount = 0;
    uint64_t serial = 0;

    bool poly() const noexcept { return patch.voiceMode == VoiceMode::poly; }
    template <typename Fn> void forEachRunningChain (Fn&&);

    void applyPatch (const Patch&, juce::MidiBuffer& out);
    void rebuild();
    void handle (const juce::MidiMessage&, juce::MidiBuffer& out, int pos);
    void inputNoteOn (int note, float vel, juce::MidiBuffer& out, int pos);
    void inputNoteOff (int note, juce::MidiBuffer& out, int pos);
    Chain& claimVoice (int note, juce::MidiBuffer& out, int pos);
    void dropVoice (int note, juce::MidiBuffer& out, int pos);
    void tick (juce::MidiBuffer& out, int pos, double dClock);
    void stepPulse (Chain&, juce::MidiBuffer& out, int pos, double dClock);
    void pulse (Chain&, float strength, juce::MidiBuffer& out, int pos);
    void noteOn (int note, float level, int deg, juce::MidiBuffer& out, int pos);
    void noteOff (int note, juce::MidiBuffer& out, int pos);
};
} // namespace mdt
