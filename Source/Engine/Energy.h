#pragma once
#include <algorithm>
#include <cmath>
#include "Model/Patch.h"
#include "Rng.h"

/** ENERGY (spec §3): one LFO per chain, output 0..1. Port of the prototype's `lfoStep`. */
namespace mdt
{
inline constexpr double twoPi = 6.283185307179586;

/** Cycle length in beats for each SYNC division: 4 bar ... 1/32. */
inline constexpr double syncBeats[8] { 16, 8, 4, 2, 1, 0.5, 0.25, 0.125 };

struct EnergySettings
{
    EnergySource source = EnergySource::osc;
    Shape        shape  = Shape::sine;
    EnergyFilter filter = EnergyFilter::off;
    EnergyEdge   edge   = EnergyEdge::both;
    bool   sync   = false;
    int    div    = 2;
    float  rateHz = 0.12f;
    float  center = 0.5f, depth = 0.7f;
    float  filterAmt = 0.3f;
};

/** What the non-oscillator sources follow, per chain. */
struct EnergyInputs
{
    float vel  = 0.7f;   // last note-on velocity, 0 after a note-off
    float note = 0.5f;   // C2..C7 -> 0..1
    float cc   = 0.5f;
};

struct Lfo
{
    double ph = 0;
    float  sh = 0.5f, lp = 0.5f, raw = 0.5f, val = 0.5f;

    /** `clock` is musical time in beats, `lfoOff` the beat this chain's synced cycle starts on. */
    void step (const EnergySettings& s, const EnergyInputs& in, double dt, double clock, double lfoOff, Rng& rng) noexcept
    {
        if (s.sync)
        {
            const double q = (clock - lfoOff) / syncBeats[std::clamp (s.div, 0, 7)];
            const double np = q - std::floor (q);
            if (np < ph) sh = rng.uniform();
            ph = np;
        }
        else
        {
            const double np = ph + s.rateHz * dt;
            if (np >= 1) sh = rng.uniform();
            ph = std::fmod (np, 1.0);
        }

        const float p = (float) ph;
        float l = 0;
        switch (s.source)
        {
            case EnergySource::vel:  l = in.vel; break;
            case EnergySource::note: l = in.note; break;
            case EnergySource::cc:   l = in.cc; break;
            case EnergySource::osc:
                l = s.shape == Shape::sine ? 0.5f - 0.5f * (float) std::cos (twoPi * p)
                  : s.shape == Shape::tri  ? (p < 0.5f ? 2 * p : 2 - 2 * p)
                  : s.shape == Shape::saw  ? p
                                           : sh;
                break;
        }

        // INERTIA / WINDOW: one time constant, 20 ms .. 3.02 s, shared by both filters. One-sided,
        // the smoother snaps on the other edge: LPF `neg` = instant onset, slow release; HPF `pos`
        // sees only rises.
        const double tau = 0.02 + (double) (s.filterAmt * s.filterAmt) * 3.0;
        if ((s.edge == EnergyEdge::pos && l < lp) || (s.edge == EnergyEdge::neg && l > lp))
            lp = l;
        else
            lp += (l - lp) * (float) (1.0 - std::exp (-dt / tau));
        const float v = s.filter == EnergyFilter::lpf ? lp
                      : s.filter == EnergyFilter::hpf ? std::clamp (0.5f + (l - lp) * 2.5f, 0.0f, 1.0f)
                                                      : l;
        raw = std::clamp (s.center + (l - 0.5f) * s.depth, 0.0f, 1.0f);
        val = std::clamp (s.center + (v - 0.5f) * s.depth, 0.0f, 1.0f);
    }
};
} // namespace mdt
