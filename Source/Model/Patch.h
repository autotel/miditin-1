#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <juce_core/juce_core.h>

/** Everything chosen rather than turned: kinds, modes, toggles, the typed arpeggio, the melody
    grid, the pitch-class set. Not automatable (CLAUDE.md). Trivially copyable on purpose -- the
    message thread hands the audio thread whole copies through a `TripleBuffer`. Defaults are the
    v2 prototype's initial state, except input-driven: an arp of the held notes (wiki/history.md). */
namespace mdt
{
enum class VoiceMode     : uint8_t { mono, poly };
enum class EnergySource  : uint8_t { osc, vel, note, cc, audio };
enum class Shape         : uint8_t { sine, tri, saw, square, sh };
enum class EnergyFilter  : uint8_t { off, lpf, hpf };
/** Which edges the energy filter acts on; the other direction passes straight through. */
enum class EnergyEdge    : uint8_t { both, pos, neg };
enum class PulseKind     : uint8_t { wheel, shaker, digital, midiIn };
/** What energy is to the wheel: spin force, or its angle. */
enum class WheelMode     : uint8_t { speed, angle };
/** What pulses under MIDI-in: each input note, or a division of the MIDI clock. */
enum class MidiInSource  : uint8_t { notes, clock };
/** MIDI clock ticks (24 a quarter) per pulse, for each `Patch::clockDiv`: 1/4 1/8 1/8T 1/16 1/16T 1/32. */
inline constexpr int clockDivTicks[6] { 24, 12, 8, 6, 4, 3 };
/** Where CLOCK counts from: the clock's own start (0xFA, or the host's bar), or each note's start. */
enum class ClockStart    : uint8_t { clock, note };
/** How energy moves the shaker's box: a hand shaking it that hard, or the box riding the curve. */
enum class ShakeMode     : uint8_t { shake, follow };
enum class GenKind       : uint8_t { arp, input, melody };
enum class Order         : uint8_t { up, down, upDown, random, asTyped };
enum class RunMode       : uint8_t { free, resetOnIn };
enum class TransformMode : uint8_t { scale, set, held };
enum class ScaleType     : uint8_t { major, minor, dorian, phrygian, pentMajor, pentMinor, wholeTone, chromatic };

inline constexpr int maxArpTones = 16;
inline constexpr int melodySteps = 16;
/** Melody cells: a tone -7..+7, or one of these. */
inline constexpr int8_t melRest = INT8_MIN;
inline constexpr int8_t melTie  = INT8_MAX;

struct Patch
{
    VoiceMode    voiceMode = VoiceMode::mono;
    /** 0 = OMNI; 1..16 = only that channel's notes are played, the rest pass through. */
    int          inChannel = 0;
    /** Mono + OMNI: the output channel. A set CH outputs on CH; poly keeps each voice's own. */
    int          outChannel = 1;

    EnergySource enSource  = EnergySource::osc;
    Shape        enShape   = Shape::sine;
    bool         enSync    = false;
    bool         enRetrig  = false;
    EnergyFilter enFilter  = EnergyFilter::off;
    EnergyEdge   enEdge    = EnergyEdge::both;
    int          ccNumber  = 1;

    PulseKind    pulse     = PulseKind::wheel;
    WheelMode    wheelMode = WheelMode::speed;
    MidiInSource midiInSource = MidiInSource::notes;
    int          clockDiv  = 3;   // index into clockDivTicks: 1/16
    ClockStart   clockStart = ClockStart::clock;
    int          digBase   = 2;   // 2, 3, 5 or 7
    ShakeMode    shakeMode = ShakeMode::shake;

    GenKind      gen       = GenKind::input;
    Order        order     = Order::up;
    std::array<int8_t, maxArpTones> arp { 0, 2, 4, 7, 9 };
    int          arpCount  = 5;
    std::array<int8_t, melodySteps> melody { 0, melTie, 2, 4, 7, melTie, melTie, 2, 0, 2, 4, 5, 4, melTie, 2, -1 };
    bool         melTranspose = false;
    RunMode      runMode   = RunMode::free;

    TransformMode tfMode   = TransformMode::held;
    int          root      = 9;   // A
    ScaleType    scale     = ScaleType::minor;
    uint16_t     pcs       = (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10);
    bool         wrap      = true;

    /** The internal preview synth (§7 Sound). Off at startup: the product is the MIDI out. */
    bool         preview   = false;

    bool operator== (const Patch&) const = default;

    // -- the typed arpeggio ---------------------------------------------------
    /** "0 2 4 7 9", "-3, 0 5": every integer in the text, clamped to ±24, at most 16. */
    void setArpText (const juce::String&);
    juce::String arpText() const;

    juce::XmlElement toXml() const;
    /** Anything missing or unreadable keeps its default. */
    static Patch fromXml (const juce::XmlElement&);
};

static_assert (std::is_trivially_copyable_v<Patch>);

/** The scale tables, as the prototype's `SCALES`. */
const std::vector<int>& scaleSteps (ScaleType);
} // namespace mdt
