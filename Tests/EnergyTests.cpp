#include <juce_core/juce_core.h>
#include "Engine/Energy.h"

using namespace mdt;

struct EnergyTests final : juce::UnitTest
{
    EnergyTests() : juce::UnitTest ("Energy") {}

    void runTest() override
    {
        const double dt = 0.001;
        Rng rng;

        beginTest ("sine: range band is center +- depth/2, a cycle per 1/rate s");
        EnergySettings s; s.rateHz = 1; s.center = 0.5f; s.depth = 0.6f;
        Lfo l;
        float lo = 1, hi = 0;
        for (int i = 0; i < 1000; ++i)
        {
            l.step (s, {}, dt, 0, 0, rng);
            lo = std::min (lo, l.val); hi = std::max (hi, l.val);
        }
        expectWithinAbsoluteError (lo, 0.2f, 0.01f);
        expectWithinAbsoluteError (hi, 0.8f, 0.01f);
        expectWithinAbsoluteError ((float) l.ph, 0.0f, 0.01f);

        beginTest ("sync: phase follows the clock, 1 bar = 4 beats");
        s.sync = true; s.div = 2; s.shape = Shape::saw; s.depth = 1;
        Lfo sy;
        sy.step (s, {}, dt, 1.0, 0, rng);
        expectWithinAbsoluteError ((float) sy.ph, 0.25f, 1e-6f);
        sy.step (s, {}, dt, 7.0, 1.0, rng);   // started on beat 1
        expectWithinAbsoluteError ((float) sy.ph, 0.5f, 1e-6f);
        expectWithinAbsoluteError (sy.val, 0.5f, 1e-6f);

        beginTest ("vel / note / cc sources pass through the band");
        EnergySettings v; v.source = EnergySource::vel; v.center = 0.5f; v.depth = 1;
        Lfo lv;
        lv.step (v, { 0.0f, 0.5f, 0.5f }, dt, 0, 0, rng);
        expectWithinAbsoluteError (lv.val, 0.0f, 1e-6f);
        v.source = EnergySource::cc;
        lv.step (v, { 0.0f, 0.5f, 0.9f }, dt, 0, 0, rng);
        expectWithinAbsoluteError (lv.val, 0.9f, 1e-6f);

        beginTest ("lpf: inertia reaches 63% of a step after one time constant");
        EnergySettings f; f.source = EnergySource::cc; f.filter = EnergyFilter::lpf; f.depth = 1; f.filterAmt = 0.5f;
        const double tau = 0.02 + 0.25 * 3;
        Lfo lf; lf.lp = 0;
        for (int i = 0; i < (int) std::round (tau / dt); ++i)
            lf.step (f, { 0, 0, 1 }, dt, 0, 0, rng);
        expectWithinAbsoluteError (lf.lp, 0.632f, 0.01f);
        expectWithinAbsoluteError (lf.raw, 1.0f, 1e-6f);   // the unfiltered line, drawn dashed

        beginTest ("hpf: rate of change -- a step spikes, a held value settles to the centre");
        f.filter = EnergyFilter::hpf;
        Lfo lh; lh.lp = 0;
        lh.step (f, { 0, 0, 1 }, dt, 0, 0, rng);
        expect (lh.val > 0.95f);
        for (int i = 0; i < 10000; ++i)
            lh.step (f, { 0, 0, 1 }, dt, 0, 0, rng);
        expectWithinAbsoluteError (lh.val, 0.5f, 0.01f);

        beginTest ("sample & hold changes only at a cycle wrap");
        EnergySettings h; h.shape = Shape::sh; h.rateHz = 2; h.depth = 1;
        Lfo lsh;
        int changes = 0;
        float last = lsh.val;
        for (int i = 0; i < 2000; ++i)
        {
            lsh.step (h, {}, dt, 0, 0, rng);
            changes += std::abs (lsh.val - last) > 1e-6f;
            last = lsh.val;
        }
        expectEquals (changes, 4);
    }
};

static EnergyTests energyTests;
