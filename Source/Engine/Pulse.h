#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "Energy.h"

/** PULSE (spec §4). Each generator is a small deterministic simulation stepped at the control
    rate; a step reports a pulse as a strength in 0..1, or a negative number for none. Ports of
    the prototype's `chainStep`. */
namespace mdt
{
/** m = offset + scale * energy, then the core parameter is scaled by m, or offset by m * range.
    Offset mode centres the energy (energy - 0.5), so the slider is the middle of the swing: with
    energy 0..1 as is (spec §4, the prototype) it only ever added, and the wheel ran flat out. */
struct EnergyMod
{
    ModMode mode = ModMode::scale;
    float   offset = 0, scale = 1;

    float apply (float energy, float base, float range, float lo, float hi) const noexcept
    {
        if (mode == ModMode::offset)
            return std::clamp (base + (offset + scale * (energy - 0.5f)) * range, lo, hi);
        return std::clamp (base * (offset + scale * energy), lo, hi);
    }
};

struct Wheel
{
    double a = 0, w = 0;   // angle, angular speed (rad/s)
    long   slot = 0;
    float  flick = 0;      // pointer flick, 1 at a peg, decays

    /** Energy is spin force: speed eases toward the target with inertia. A peg passing the
        pointer is a pulse, stronger the faster the wheel turns. */
    float step (float targetRevPerSec, int slices, double dt) noexcept
    {
        const double target = targetRevPerSec * twoPi;
        w += (target - w) * std::min (1.0, 0.8 * dt);
        a += w * dt;
        flick = std::max (0.0f, flick - (float) dt * 7);
        const long s = (long) std::floor (a / (twoPi / slices));
        if (s == slot)
            return -1;
        slot = s;
        flick = 1;
        return std::clamp (0.3f + (float) (w / (twoPi * 1.4)), 0.0f, 1.0f);
    }

    void kick() noexcept { w += twoPi * 1.2; }
};

struct Ball { float x = 0.5f, y = 0.5f, vx = 0, vy = 0, cool = 0; double hit = -9; };
struct WallHit { char wall = 0; float pos = 0, speed = 0; double t = -9; };

struct Shaker
{
    static constexpr int maxBalls = 12;
    static constexpr float radius = 0.055f;
    std::array<Ball, maxBalls> balls {};
    int count = 0;
    float effort = 0;   // the agitation actually applied, for the readout
    std::array<WallHit, 8> walls {};   // ring of recent wall hits, for drawing
    int wallNext = 0;
    /** The box's displacement and velocity, box widths (and per second). Balls live in the box's
        frame, so they feel gravity and minus its acceleration -- the same push for every ball. */
    float boxX = 0, boxY = 0, boxVx = 0, boxVy = 0;
    double shakePh = 0;
    float ampNow = 0;                          // shake amplitude, eased so a jump is no blow
    float baseX = 0, baseY = 0;                // re-bases a mode switch, drifts back to centre
    ShakeMode lastMode = ShakeMode::shake;

    /** `agitation` is the energy-modulated core value. SHAKE: it sets how hard a hand shakes the
        box (~4 Hz across, half that up and down). FOLLOW: the box rides it vertically, so only its
        changes stir the balls. Only wall hits pulse; impact speed sets strength. `onPulse(strength)`
        per hit. */
    template <typename OnPulse>
    void step (float agitation, ShakeMode mode, int wanted, float bounce, double t, double dt, Rng& rng, OnPulse&& onPulse)
    {
        wanted = std::clamp (wanted, 1, maxBalls);
        while (count < wanted)
            balls[(size_t) count++] = { 0.2f + rng.uniform() * 0.6f, 0.2f + rng.uniform() * 0.6f, rng.uniform() - 0.5f, 0, 0, -9 };
        count = wanted;

        const float ag = agitation;
        effort = ag;
        const float ease = (float) (1.0 - std::exp (-dt / followTau));
        if (mode != lastMode)
            ampNow = 0;
        float nx = 0, ny;
        if (mode == ShakeMode::shake)
        {
            ampNow += (std::pow (ag, 1.4f) - ampNow) * ease;
            shakePh += twoPi * shakeHz * dt;
            nx = shakeAmp * ampNow * (float) std::sin (shakePh);
            ny = 0.6f * shakeAmp * ampNow * (float) std::sin (0.5 * shakePh + 1.0);
        }
        else
        {
            // Smoothed, so a square edge or an S&H jump is a fast move rather than an infinite one.
            const float target = -followTravel * (ag - 0.5f);
            ny = (mode != lastMode ? target : boxY - baseY + (target - (boxY - baseY)) * ease);
        }
        // A mode switch changes the box's frame of reference, not where it is: take up the
        // difference in the base, which then drifts home too slowly to push anything.
        if (mode != lastMode)
        {
            baseX = boxX - nx;
            baseY = boxY - ny;
            lastMode = mode;
        }
        const float home = (float) std::exp (-dt / baseTau);
        baseX *= home;
        baseY *= home;
        nx += baseX;
        ny += baseY;
        const float vx = (nx - boxX) / (float) dt, vy = (ny - boxY) / (float) dt;
        const float pushX = vx - boxVx, pushY = vy - boxVy;
        boxX = nx; boxY = ny; boxVx = vx; boxVy = vy;

        const float damp = (float) std::exp (-0.7 * dt);
        const float R = radius;

        auto hit = [&] (Ball& b, char w, float pos, float sp)
        {
            if (sp > 0.35f && b.cool <= 0)
            {
                b.cool = 0.06f;
                b.hit = t;
                walls[(size_t) wallNext] = { w, pos, sp, t };
                wallNext = (wallNext + 1) % (int) walls.size();
                onPulse (std::clamp (sp / 3, 0.15f, 1.0f));
            }
        };

        for (int i = 0; i < count; ++i)
        {
            auto& b = balls[(size_t) i];
            b.vx -= pushX;
            b.vy -= pushY - gravity * (float) dt;
            b.vx *= damp;
            b.vy *= damp;
            const float sp = std::hypot (b.vx, b.vy);
            if (sp > 4) { b.vx *= 4 / sp; b.vy *= 4 / sp; }
            b.x += b.vx * (float) dt;
            b.y += b.vy * (float) dt;
            b.cool -= (float) dt;
            if (b.x < R)     { b.x = R;     hit (b, 'l', b.y, std::abs (b.vx)); b.vx =  std::abs (b.vx) * bounce; }
            if (b.x > 1 - R) { b.x = 1 - R; hit (b, 'r', b.y, std::abs (b.vx)); b.vx = -std::abs (b.vx) * bounce; }
            if (b.y < R)     { b.y = R;     hit (b, 't', b.x, std::abs (b.vy)); b.vy =  std::abs (b.vy) * bounce; }
            if (b.y > 1 - R) { b.y = 1 - R; hit (b, 'b', b.x, std::abs (b.vy)); b.vy = -std::abs (b.vy) * bounce; }
        }

        for (int i = 0; i < count; ++i)
            for (int j = i + 1; j < count; ++j)
            {
                auto& a = balls[(size_t) i];
                auto& b = balls[(size_t) j];
                const float dx = b.x - a.x, dy = b.y - a.y, d = std::hypot (dx, dy);
                if (d < 2 * R && d > 0)
                {
                    const float ux = dx / d, uy = dy / d, ov = (2 * R - d) / 2;
                    a.x -= ux * ov; a.y -= uy * ov; b.x += ux * ov; b.y += uy * ov;
                    const float rel = (a.vx - b.vx) * ux + (a.vy - b.vy) * uy;
                    if (rel > 0) { a.vx -= rel * ux; a.vy -= rel * uy; b.vx += rel * ux; b.vy += rel * uy; }
                }
            }
    }

    /** The SHAKE button: one jolt of the box in a random direction, felt by every ball alike. */
    void kick (Rng& rng) noexcept
    {
        const double a = rng.uniform() * twoPi;
        for (int i = 0; i < count; ++i)
        {
            balls[(size_t) i].vx += 3 * (float) std::cos (a);
            balls[(size_t) i].vy += 3 * (float) std::sin (a);
        }
    }

    // ponytail: hand-tuned feel constants; knobs if one setting cannot serve every patch.
    static constexpr float  gravity = 2.6f, shakeAmp = 0.08f, followTravel = 0.5f;
    static constexpr double shakeHz = 4.0, followTau = 0.02, baseTau = 2.0;
};

struct Digital
{
    double beat = 0;   // beats since this chain's bar started
    int    div  = 2;   // the grid row energy picked, 0..4
    double lastPulseT = -9;

    /** Beats per division at row `k`: a bar of 4 beats over base * 2^k. */
    static double cell (int base, int k) noexcept { return 4.0 / (base * (1 << std::clamp (k, 0, 4))); }

    /** Divisions per bar are base * 2^k; energy picks k, so it is always on a grid. Swing delays
        every odd cell, base 2 at 1/16 or finer only. */
    float step (double dBeat, int base, int k, float swing) noexcept
    {
        const double prev = beat;
        beat += dBeat;
        div = std::clamp (k, 0, 4);
        const double d = cell (base, div);
        auto sw = [&] (double x)
        {
            const double q = x / d, cellIndex = std::floor (q), f = q - cellIndex;
            if (base != 2 || d > 0.25 || swing <= 0) return cellIndex;
            return (((long long) cellIndex & 1) && f < swing * 0.5) ? cellIndex - 0.5 : cellIndex;
        };
        if (std::floor (sw (beat)) <= std::floor (sw (prev)))
            return -1;
        const bool onBeat = std::abs (beat - std::round (beat)) < 0.04;
        return onBeat ? 0.95f : 0.6f;
    }
};
} // namespace mdt
