#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include "Model/Patch.h"

/** TRANSFORM (spec §6): relative tone -> MIDI note. A pure function of (tone, config, held notes),
    shared by every voice. Port of the prototype's `pool()` / `transform()`.

    The **pool** is the sorted set of pitches one period holds, plus a border note `base`:
    - scale: the scale's steps + root, base = (whole octave + 1) * 12, period 12
    - set:   the pitch classes measured up from base = round((octave + 1) * 12), period OCT LEN
    - held:  the held notes themselves, base 0, period 12
    Wrap on: tone d sits in period floor(d / n), so tones run on past the pool. Wrap off: tones fold
    back into the one period. */
namespace mdt
{
struct TransformConfig
{
    TransformMode mode   = TransformMode::scale;
    int           root   = 9;
    ScaleType     scale  = ScaleType::minor;
    uint16_t      pcs    = 0;
    float         octave = 4.0f;   // fractional only matters in set mode
    int           period = 12;     // OCT LEN, set mode only
    bool          wrap   = true;
};

struct Pool
{
    std::array<int, 128> p {};
    int n = 0, base = 0, period = 12;
    bool wrap = true;

    static Pool make (const TransformConfig& c, const int* held, int heldCount)
    {
        Pool r;
        r.wrap = c.wrap;
        if (c.mode == TransformMode::scale)
        {
            r.base = ((int) std::floor (c.octave) + 1) * 12;
            for (int s : scaleSteps (c.scale))
                r.p[(size_t) r.n++] = s + c.root;
        }
        else if (c.mode == TransformMode::set)
        {
            r.base = (int) std::lround ((c.octave + 1.0f) * 12.0f);
            r.period = std::max (1, c.period);
            for (int pc = 0; pc < 12; ++pc)
                if (c.pcs & (1 << pc))
                    r.p[(size_t) r.n++] = mod (pc - r.base, 12);
        }
        else
        {
            for (int i = 0; i < std::min (heldCount, 128); ++i)
                r.p[(size_t) r.n++] = held[i];
        }
        std::sort (r.p.begin(), r.p.begin() + r.n);
        return r;
    }

    /** The pitch tone `d` lands on before range checking -- may be outside 0..127. */
    int tone (int d) const noexcept
    {
        if (! wrap)
            return base + p[(size_t) mod (d, n)];
        const int o = floorDiv (d, n);
        return base + p[(size_t) (d - o * n)] + period * o;
    }

    struct Located { int k = 0, shift = 0; };

    /** The inverse (§6.5): the pool index `k` of a note, so `tone(k) == note` for a pool note.
        Wrap on: the highest index whose pitch is <= the note. Wrap off: the index nearest below in
        pitch class, plus the whole-octave `shift` that carries the folded period to the note. */
    Located locate (int note) const noexcept
    {
        Located r;
        if (wrap)
        {
            const int est = floorDiv (note - base - p[0], period) * n;
            r.k = est - 2 * n;
            for (int j = est - 2 * n; j <= est + 2 * n; ++j)
                if (tone (j) <= note)
                    r.k = j;
            return r;
        }
        int best = 1 << 30;
        for (int j = 0; j < n; ++j)
        {
            const int dd = mod (note - tone (j), 12);
            if (dd < best) { best = dd; r.k = j; }
        }
        r.shift = 12 * (int) std::floor ((double) (note - tone (r.k)) / 12.0 + 0.5);
        return r;
    }

    /** Tone -> note. With a reference note, tone 0 lands on it (poly voices, TRANSPOSE BY IN). */
    std::optional<int> transform (int d, std::optional<int> ref = {}) const noexcept
    {
        if (n == 0)
            return {};
        int shift = 0;
        if (ref)
        {
            const auto at = locate (*ref);
            d += at.k;
            shift = at.shift;
        }
        const int m = tone (d) + shift;
        if (m < 0 || m > 127)
            return {};
        return m;
    }

    static int mod (int a, int b) noexcept { return ((a % b) + b) % b; }
    static int floorDiv (int a, int b) noexcept { return (a - mod (a, b)) / b; }
};
} // namespace mdt
