#include "ParamRegistry.h"
#include <cmath>
#include <cstdio>

namespace mdt
{
// Ranges and defaults are the v2 prototype's: its `state` initialiser and each <input type=range>.
const std::array<ParamSpec, numParams>& params()
{
    static const std::array<ParamSpec, numParams> table { {
        { "en_rate",    "Energy rate",              "RATE",    0.02f, 3.0f,   0.12f, 0, Fmt::hertz, true },
        { "en_div",     "Energy sync cycle",        "CYCLE",   0, 7,  2, 1, Fmt::syncDiv },
        { "en_center",  "Energy center",            "CENTER",  0, 1,  0.5f, 0, Fmt::plain },
        { "en_depth",   "Energy depth",             "DEPTH",   0, 1,  0.7f, 0, Fmt::plain },
        { "en_filter",  "Energy filter time",       "INERTIA", 0, 1,  0.3f, 0, Fmt::filterTime },
        { "wh_speed",   "Wheel speed",              "SPEED",   0.05f, 2, 0.7f, 0, Fmt::plain },
        { "wh_slices",  "Wheel slices",             "SLICES",  3, 24, 12, 1, Fmt::integer },
        { "sh_agitate", "Shaker agitation",         "AGITATE", 0, 1,  1, 0, Fmt::plain },
        { "sh_balls",   "Shaker balls",             "BALLS",   1, 12, 5, 1, Fmt::integer },
        { "sh_bounce",  "Shaker bounce",            "BOUNCE",  0.3f, 0.98f, 0.82f, 0, Fmt::plain },
        { "dg_div",     "Digital grid division",    "DIV",     0, 4,  4, 1, Fmt::gridDiv },
        { "dg_swing",   "Digital swing",            "SWING",   0, 0.6f, 0, 0, Fmt::percent },
        { "wh_mod_off", "Wheel energy offset",      "OFFSET", -1, 1,  0, 0, Fmt::plain },
        { "wh_mod_scl", "Wheel energy scale",       "SCALE",  -2, 2,  1, 0, Fmt::times },
        { "sh_mod_off", "Shaker energy offset",     "OFFSET", -1, 1,  0, 0, Fmt::plain },
        { "sh_mod_scl", "Shaker energy scale",      "SCALE",  -2, 2,  1, 0, Fmt::times },
        { "dg_mod_off", "Digital energy offset",    "OFFSET", -1, 1,  0, 0, Fmt::plain },
        { "dg_mod_scl", "Digital energy scale",     "SCALE",  -2, 2,  1, 0, Fmt::times },
        { "mel_len",    "Melody length",            "LENGTH",  2, 16, 16, 1, Fmt::integer },
        { "tf_octave",  "Transform octave",         "OCTAVE",  1, 7,  4, 0, Fmt::plain },
        { "tf_octlen",  "Transform octave length",  "OCT LEN", 1, 24, 12, 1, Fmt::semitones },
        { "gate",       "Gate",                     "GATE",    30, 1200, 180, 10, Fmt::ms },
        { "bpm",        "Tempo without host",       "BPM",     60, 200, 112, 1, Fmt::integer },
    } };
    return table;
}

std::string format (P p, float v, int context)
{
    static const char* const syncLabels[] { "4 bar", "2 bar", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32" };
    char b[32];
    switch (spec (p).fmt)
    {
        case Fmt::plain:      std::snprintf (b, sizeof b, "%.2f", (double) v); break;
        case Fmt::integer:    std::snprintf (b, sizeof b, "%d", (int) std::lround (v)); break;
        case Fmt::hertz:      std::snprintf (b, sizeof b, "%.2fHz", (double) v); break;
        case Fmt::ms:         std::snprintf (b, sizeof b, "%dms", (int) std::lround (v)); break;
        case Fmt::percent:    std::snprintf (b, sizeof b, "%d%%", (int) std::lround (v * 100)); break;
        case Fmt::times:      std::snprintf (b, sizeof b, "\xc3\x97%.2f", (double) v); break;
        case Fmt::semitones:  std::snprintf (b, sizeof b, "%dst", (int) std::lround (v)); break;
        case Fmt::filterTime: std::snprintf (b, sizeof b, "%.2fs", 0.02 + (double) (v * v) * 3.0); break;
        case Fmt::syncDiv:    return syncLabels[std::clamp ((int) std::lround (v), 0, 7)];
        case Fmt::gridDiv:    std::snprintf (b, sizeof b, "1/%d", context * (1 << std::clamp ((int) std::lround (v), 0, 4))); break;
    }
    return b;
}
} // namespace mdt
