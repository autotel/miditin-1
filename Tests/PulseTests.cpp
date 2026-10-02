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

        beginTest ("shaker STRENGTH: deterministic for a seed, only wall hits pulse, settles when calm");
        auto run = [&] (auto&& agAt, ShakeMode mode, uint32_t seed, int from = 0, int ticks = 5000) {
            Shaker s; Rng r (seed);
            std::vector<double> hits;
            for (int i = 0; i < ticks; ++i)
                s.step (agAt (i * dt), mode, 5, 0.82f, i * dt, dt, r, [&] (float v) {
                    expect (v >= 0.15f && v <= 1.0f);
                    if (i >= from) hits.push_back (i * dt);
                });
            return hits;
        };
        const auto full = [] (double) { return 1.0f; };
        const auto calm = [] (double) { return 0.0f; };
        const auto a = run (full, ShakeMode::shake, 7), b = run (full, ShakeMode::shake, 7);
        expect (a == b, "repeat");
        expect (a.size() > 10, juce::String ((int) a.size()));
        expect (run (full, ShakeMode::shake, 8) != a, "seed");
        expect (run (calm, ShakeMode::shake, 7, 3000).size() < a.size() / 10, "unshaken balls mostly lie still");

        beginTest ("shaker POSITION: a steady curve settles, a square curve jolts at its edges");
        expect (run ([] (double) { return 0.8f; }, ShakeMode::follow, 7, 3000).size() < a.size() / 10, "a box held still lets the balls lie");
        const auto sq = run ([] (double t) { return std::fmod (t, 1.0) < 0.5 ? 1.0f : 0.0f; }, ShakeMode::follow, 7, 2000, 6000);
        for (double edge = 2.0; edge < 6.0; edge += 0.5)   // every edge throws the balls at a wall
            expect (std::any_of (sq.begin(), sq.end(), [edge] (double t) { return t >= edge && t < edge + 0.1; }),
                    "edge at " + juce::String (edge));

        beginTest ("shaker: a mode switch or an energy jump moves no ball by itself");
        {
            // Balls at rest on the floor, then the box changes frame (mode) or amplitude (S&H jump).
            for (int scenario = 0; scenario < 2; ++scenario)
            {
                Shaker s; Rng r (7);
                int burst = 0;
                for (int i = 0; i < 8000; ++i)
                {
                    const bool after = i >= 6000;
                    const auto mode = scenario == 0 && after ? ShakeMode::follow : ShakeMode::shake;
                    const float ag = scenario == 0 ? (after ? 1.5f : 0.0f) : (after ? 0.4f : 0.0f);
                    s.step (ag, mode, 5, 0.82f, i * dt, dt, r, [&] (float) { burst += i >= 6000 && i < 6030; });
                }
                expectEquals (burst, 0, "scenario " + juce::String (scenario));
            }
        }

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
