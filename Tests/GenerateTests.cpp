#include <juce_core/juce_core.h>
#include "Engine/Generate.h"

using namespace mdt;

struct GenerateTests final : juce::UnitTest
{
    GenerateTests() : juce::UnitTest ("Generate") {}

    std::vector<int> seqOf (const Patch& p, bool poly = false, std::vector<int> held = {})
    {
        const auto s = buildSequence (p, poly, held.data(), (int) held.size());
        return { s.v.begin(), s.v.begin() + s.n };
    }

    std::vector<int> play (const Patch& p, int steps, std::vector<int> held = {}, int melLen = 16)
    {
        const auto s = buildSequence (p, false, held.data(), (int) held.size());
        GenPos g;
        Rng r;
        std::vector<int> out;
        for (int i = 0; i < steps; ++i)
        {
            const auto st = g.advance (p, s, melLen, r);
            out.push_back (st.kind == GenStep::tone ? st.value : st.kind == GenStep::tie ? 100 : -100);
        }
        return out;
    }

    void runTest() override
    {
        Patch p;
        p.setArpText ("4 0 -2 7");

        beginTest ("orders");
        p.order = Order::up;      expect (seqOf (p) == std::vector<int> { -2, 0, 4, 7 });
        p.order = Order::down;    expect (seqOf (p) == std::vector<int> { 7, 4, 0, -2 });
        p.order = Order::upDown;  expect (seqOf (p) == std::vector<int> { -2, 0, 4, 7, 4, 0 });
        p.order = Order::asTyped; expect (seqOf (p) == std::vector<int> { 4, 0, -2, 7 });
        p.setArpText ("3 1");
        p.order = Order::upDown;  expect (seqOf (p) == std::vector<int> { 1, 3 });

        beginTest ("one step per pulse, wrapping");
        p.setArpText ("0 2 4"); p.order = Order::up;
        expect (play (p, 5) == std::vector<int> { 0, 2, 4, 0, 2 });

        beginTest ("random order is seeded, stays in the sequence");
        p.order = Order::random;
        const auto a = play (p, 50), b = play (p, 50);
        expect (a == b);
        for (int v : a) expect (v == 0 || v == 2 || v == 4);

        beginTest ("input: held notes as indices, as played; poly is the single step 0");
        Patch in; in.gen = GenKind::input; in.order = Order::asTyped;
        expect (seqOf (in, false, { 64, 57, 60 }) == std::vector<int> { 2, 0, 1 });
        in.order = Order::down;
        expect (seqOf (in, false, { 64, 57, 60 }) == std::vector<int> { 2, 1, 0 });
        expect (seqOf (in, true, { 64, 57, 60 }) == std::vector<int> { 0 });
        expect (seqOf (in, false, {}).empty());

        beginTest ("melody: tones, ties, rests, length");
        Patch m; m.gen = GenKind::melody;
        m.melody.fill (melRest);
        m.melody[0] = 3; m.melody[1] = melTie; m.melody[3] = -7;
        expect (play (m, 5, {}, 4) == std::vector<int> { 3, 100, -100, -7, 3 });

        beginTest ("nextIsTie looks one step ahead, across the wrap");
        GenPos g; Rng r; ToneSeq none;
        g.advance (m, none, 4, r);
        expect (g.nextIsTie (m, 4));
        g.advance (m, none, 4, r);
        expect (! g.nextIsTie (m, 4));
        m.melody[0] = melTie;
        g.advance (m, none, 4, r); g.advance (m, none, 4, r);   // at step 3
        expect (g.nextIsTie (m, 4));
    }
};

static GenerateTests generateTests;
