#pragma once
#include <algorithm>
#include <array>
#include "Model/Patch.h"
#include "Rng.h"

/** GENERATE (spec §5): one step per pulse, emitting relative tones. Port of the prototype's
    `arpSeq` / `genStep`; the melody's held notes are the engine's job, since they reach into the
    output note table. */
namespace mdt
{
struct ToneSeq
{
    std::array<int, 256> v {};   // up/down of 128 held notes is 254
    int n = 0;
};

/** The arpeggio's sequence in play order. Input mode builds it from the held notes (press order),
    each as its index among them sorted, so "as typed" becomes "as played"; in poly it is the one
    step "this note". */
inline ToneSeq buildSequence (const Patch& p, bool poly, const int* held, int heldCount)
{
    ToneSeq raw;
    if (p.gen == GenKind::input && poly)
    {
        raw.v[0] = 0;
        raw.n = 1;
    }
    else if (p.gen == GenKind::input)
    {
        std::array<int, 128> sorted {};
        std::copy (held, held + heldCount, sorted.begin());
        std::sort (sorted.begin(), sorted.begin() + heldCount);
        for (int i = 0; i < heldCount; ++i)
            raw.v[(size_t) raw.n++] = (int) (std::find (sorted.begin(), sorted.begin() + heldCount, held[i]) - sorted.begin());
    }
    else
    {
        for (int i = 0; i < p.arpCount; ++i)
            raw.v[(size_t) raw.n++] = p.arp[(size_t) i];
    }

    if (p.order == Order::asTyped)
        return raw;

    ToneSeq up = raw;
    std::sort (up.v.begin(), up.v.begin() + up.n);
    if (p.order == Order::down)
        std::reverse (up.v.begin(), up.v.begin() + up.n);
    else if (p.order == Order::upDown && up.n > 2)
        // 0 2 4 7 -> 0 2 4 7 4 2: the ends are not repeated.
        for (int i = raw.n - 2; i >= 1; --i)
            up.v[(size_t) up.n++] = up.v[(size_t) i];
    return up;
}

struct GenStep
{
    enum Kind { rest, tone, tie } kind = rest;
    int value = 0;
};

struct GenPos
{
    int arpI = -1, arpPrev = -1, melI = -1;

    void reset() noexcept { arpI = arpPrev = melI = -1; }

    GenStep advance (const Patch& p, const ToneSeq& seq, int melLength, Rng& rng) noexcept
    {
        if (p.gen != GenKind::melody)
        {
            if (seq.n == 0)
                return {};
            arpPrev = arpI;
            arpI = p.order == Order::random ? (int) (rng.next() % (uint32_t) seq.n) : (arpI + 1) % seq.n;
            return { GenStep::tone, seq.v[(size_t) arpI] };
        }
        melI = (melI + 1) % melLength;
        const auto v = p.melody[(size_t) melI];
        if (v == melTie)  return { GenStep::tie, 0 };
        if (v == melRest) return {};
        return { GenStep::tone, v };
    }

    /** Whether the step after the current one continues a held note. */
    bool nextIsTie (const Patch& p, int melLength) const noexcept
    {
        return p.gen == GenKind::melody && melI >= 0 && p.melody[(size_t) ((melI + 1) % melLength)] == melTie;
    }
};
} // namespace mdt
