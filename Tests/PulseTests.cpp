#include <juce_core/juce_core.h>
#include "Engine/Pulse.h"

using namespace mdt;

struct PulseTests final : juce::UnitTest
{
    PulseTests() : juce::UnitTest ("Pulse") {}

    void runTest() override
    {
        const double dt = 0.001;

        beginTest ("energy modulation: scale and offset modes");
        EnergyMod m;
        expectWithinAbsoluteError (m.apply (0.5f, 0.7f, 2, 0, 3), 0.35f, 1e-6f);
        m.mode = ModMode::offset; m.offset = -0.5f; m.scale = 2;
        expectWithinAbsoluteError (m.apply (0.75f, 0.7f, 2, 0, 3), 0.7f + 1.0f * 2, 1e-6f);
        expectWithinAbsoluteError (m.apply (0.0f, 0.7f, 2, 0, 3), 0.0f, 1e-6f);   // clamped

        beginTest ("wheel: at speed, slices x rev/s pulses per second, stronger when faster");
        Wheel w;
        int n = 0;
        float strength = 0;
        for (int i = 0; i < 20000; ++i)
            if (float s = w.step (0.5f, 12, dt); s >= 0 && i >= 10000) { ++n; strength = s; }
        expect (std::abs (n - 60) <= 1, juce::String (n));
        expectWithinAbsoluteError (strength, 0.3f + 0.5f / 1.4f, 0.01f);

        beginTest ("wheel: speed eases in -- no pulse in the first instant from rest");
        Wheel rest;
        int early = 0;
        for (int i = 0; i < 100; ++i) early += rest.step (2.0f, 3, dt) >= 0;
        expectEquals (early, 0);

        beginTest ("shaker: deterministic for a seed, only wall hits pulse, settles when calm");
        auto run = [&] (float ag, uint32_t seed, int from = 0) {
            Shaker s; Rng r (seed);
            std::vector<float> hits;
            for (int i = 0; i < 5000; ++i)
                s.step (ag, 5, 0.82f, i * dt, dt, r, [&] (float v) { if (i >= from) hits.push_back (v); });
            return hits;
        };
        const auto a = run (1, 7), b = run (1, 7);
        expect (a == b, "repeat");
        expect (a.size() > 10, juce::String ((int) a.size()));
        for (float v : a) expect (v >= 0.15f && v <= 1.0f);
        expect (run (1, 8) != a, "seed");
        expect (run (0, 7, 3000).size() < run (0, 7).size() / 4);   // dropped balls bounce, then mostly lie still

        beginTest ("digital: base x 2^k pulses per bar, downbeats stronger");
        for (int base : { 2, 3, 5, 7 })
            for (int k = 0; k < 5; ++k)
            {
                Digital d;
                int count = 0, strong = 0;
                const double dBeat = 1.0 / 1000;   // 60 bpm in beats per tick
                for (int i = 0; i < 8000; ++i)       // two bars
                    if (float s = d.step (dBeat, base, k, 0); s >= 0) { ++count; strong += s > 0.9f; }
                expect (std::abs (count - 2 * base * (1 << k)) <= 1, juce::String (base) + " " + juce::String (k) + ": " + juce::String (count));
                expect (strong >= 1);
            }

        beginTest ("digital swing: base 2 at 1/16 delays odd cells, never changes the count");
        Digital straight, swung;
        std::vector<int> a1, a2;
        for (int i = 0; i < 4000; ++i)
        {
            if (straight.step (0.001, 2, 3, 0) >= 0) a1.push_back (i);
            if (swung.step (0.001, 2, 3, 0.5f) >= 0) a2.push_back (i);
        }
        expectEquals ((int) a1.size(), (int) a2.size());
        expectEquals (a2[1] - a1[1], 0);                      // even cell on time
        expect (std::abs (a2[0] - a1[0] - 62.5) <= 1.0);      // odd cell: 0.25 beat * 0.25 = 62.5 ms
    }
};

static PulseTests pulseTests;
