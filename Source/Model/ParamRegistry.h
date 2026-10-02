#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

/** The one table. Every host parameter is described here once -- id, host name, range, default,
    unit -- and the APVTS layout, the UI's sliders and their readouts all derive from it.

    Only the prototype's sliders live here; choices and toggles are `Patch` (see
    wiki/architecture.md, "Host parameters vs Patch"). Values are natural units throughout. */
namespace mdt
{
enum class P
{
    enRate, enDiv, enCenter, enDepth, enFilterAmt, enPhase, enHpfZero,
    whSpeed, whSlices, shAgitate, shBalls, shBounce, dgDiv, dgSwing,
    whModOffset, whModScale, shModOffset, shModScale, dgModOffset, dgModScale,
    melLength,
    tfOctave, tfOctLen,
    gate, velEnergy, velPulse, velInput, bpm,
    count
};
inline constexpr int numParams = (int) P::count;

enum class Fmt { plain, integer, hertz, ms, percent, times, syncDiv, gridDiv, semitones, filterTime, degrees };

struct ParamSpec
{
    const char* id;     // host-facing and saved; never rename casually
    const char* name;   // host-facing, unique out of context
    const char* label;  // the UI's short label, as the prototype writes it
    float min, max, def;
    float step;         // 0 = continuous; 1 = integer
    Fmt   fmt;
    bool  logCurve = false;
};

const std::array<ParamSpec, numParams>& params();
inline const ParamSpec& spec (P p) { return params()[(size_t) p]; }

/** The readout under a slider, and the host's display string. `syncDiv` / `gridDiv` need the
    digital base for the latter, passed as `context`. */
std::string format (P, float natural, int context = 2);

inline float toNatural (const ParamSpec& s, float v01)
{
    v01 = v01 < 0.0f ? 0.0f : (v01 > 1.0f ? 1.0f : v01);
    return s.logCurve ? s.min * std::pow (s.max / s.min, v01) : s.min + v01 * (s.max - s.min);
}

inline float toNormalised (const ParamSpec& s, float natural)
{
    const float r = s.logCurve ? std::log (natural / s.min) / std::log (s.max / s.min)
                               : (natural - s.min) / (s.max - s.min);
    return r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
}

/** Every parameter's current natural value, as the engine reads them once per block. */
struct ParamValues
{
    std::array<float, numParams> v {};
    float operator[] (P p) const noexcept { return v[(size_t) p]; }
    float& operator[] (P p) noexcept { return v[(size_t) p]; }
    int   i (P p) const noexcept { return (int) std::lround (v[(size_t) p]); }

    static ParamValues defaults()
    {
        ParamValues d;
        for (int k = 0; k < numParams; ++k)
            d.v[(size_t) k] = params()[(size_t) k].def;
        return d;
    }
};
} // namespace mdt
