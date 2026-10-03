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
/** The core parameter scaled by m = offset + scale * energy (spec §4's SCALE mode; the OFFSET
    mode went for per-generator energy modes, wiki/history.md). */
struct EnergyMod
{
    float offset = 0, scale = 1;

    float apply (float energy, float base, float lo, float hi) const noexcept
    {
        return std::clamp (base * (offset + scale * energy), lo, hi);
    }
};

struct Wheel
{
    double a = 0, w = 0;   // angle, angular speed (rad/s)
    long   slot = 0;
    float  flick = 0;      // pointer flick, 1 at a peg, decays
    double angleBase = 0;  // ANGLE: where energy 0 points, re-based on a switch so nothing jumps
    WheelMode lastMode = WheelMode::speed;
    int    lastSlices = 0;
    float  strength = 0;   // of this step's pulses

    /** SPEED: `value` is rev/s, the spin force -- speed eases toward it with inertia. ANGLE:
        `value` is turns, and the wheel is sprung (critically damped) to that angle, so pegs pass
        only while the energy moves. A peg passing the pointer is a pulse, stronger the faster the
        wheel turns, either way. Returns how many pegs passed this step -- all of them pulse. */
    int step (float value, WheelMode mode, int slices, double dt) noexcept
    {
        const double target = value * twoPi;
        if (mode == WheelMode::angle)
        {
            if (lastMode != mode)
                angleBase = a - target;
            w += (springW * springW * (angleBase + target - a) - 2 * springW * w) * dt;
        }
        else
        {
            w += (target - w) * std::min (1.0, 0.8 * dt);
        }
        lastMode = mode;
        a += w * dt;
        flick = std::max (0.0f, flick - (float) dt * 7);
        const long s = (long) std::floor (a / (twoPi / slices));
        if (slices != lastSlices)   // a SLICES edit moves every peg: resync, pulse nothing
        {
            lastSlices = slices;
            slot = s;
        }
        const int pegs = (int) std::min (std::abs (s - slot), (long) slices);
        if (pegs == 0)
            return 0;
        slot = s;
        flick = 1;
        strength = std::clamp (0.3f + (float) (std::abs (w) / (twoPi * 1.4)), 0.0f, 1.0f);
        return pegs;
    }

    void kick() noexcept { w += twoPi * 1.2; }

    // ponytail: hand-tuned spring (settles in ~0.3 s); a knob if ANGLE wants more or less lag.
    static constexpr double springW = 12.0;
};

struct Ball
{
    float x = 0.5f, y = 0.5f, vx = 0, vy = 0, cool = 0;
    double hit = -9;
    /** The last positions, newest first, one every `trailEvery` ticks: the path it drew. */
    static constexpr int trailLength = 10, trailEvery = 15;   // 150 ms
    std::array<float, trailLength> tx {}, ty {};
    int trailCount = 0;

    void recordTrail() noexcept
    {
        for (int i = std::min (trailCount, trailLength - 1); i > 0; --i)
        {
            tx[(size_t) i] = tx[(size_t) i - 1];
            ty[(size_t) i] = ty[(size_t) i - 1];
        }
        tx[0] = x;
        ty[0] = y;
        trailCount = std::min (trailCount + 1, trailLength);
    }
};
struct WallHit { char wall = 0; float pos = 0, speed = 0; double t = -9; };

struct Shaker
{
    static constexpr int maxBalls = 12;
    static constexpr float radius = 0.055f;
    std::array<Ball, maxBalls> balls {};
    int count = 0;
    float effort = 0;   // the agitation actually applied, for the readout
    std::array<WallHit, 24> walls {};  // ring of recent wall hits, for drawing (0.4 s of them)
    int wallNext = 0;
    /** The box's displacement and velocity, box widths (and per second). Balls live in the box's
        frame, so they feel gravity and minus its acceleration -- the same push for every ball. */
    float boxX = 0, boxY = 0, boxVx = 0, boxVy = 0;
    double shakePh = 0;
    float ampNow = 0;                          // shake amplitude, eased so a jump is no blow
    float baseX = 0, baseY = 0;                // re-bases a mode switch, drifts back to centre
    ShakeMode lastMode = ShakeMode::shake;
    int trailTick = 0;

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
            // x sways a full sine over the energy range: not a line through y, so the push turns
            // as energy moves and the balls cannot settle into vertical lanes.
            const float target = -followTravel * (ag - 0.5f);
            const float targetX = followSway * (float) std::sin (twoPi * (ag - 0.5f));
            ny = (mode != lastMode ? target : boxY - baseY + (target - (boxY - baseY)) * ease);
            nx = (mode != lastMode ? targetX : boxX - baseX + (targetX - (boxX - baseX)) * ease);
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
            if (sp > 0.35f)
                b.recordTrail();   // a vertex at the wall, so the drawn path bends there
            if (sp > 0.35f && b.cool <= 0)
            {
                b.cool = 0.02f;   // a corner rattle is one hit; a real second bounce is not
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
            // A sanity cap only, relative to the box: the prototype's 4 /s held balls to the box in a
            // fast move, so they rode along instead of being hit. 40 /s still cannot tunnel a wall.
            if (sp > maxSpeed) { b.vx *= maxSpeed / sp; b.vy *= maxSpeed / sp; }
            b.x += b.vx * (float) dt;
            b.y += b.vy * (float) dt;
            b.cool -= (float) dt;
            if (b.x < R)     { b.x = R;     hit (b, 'l', b.y, std::abs (b.vx)); b.vx =  std::abs (b.vx) * bounce; }
            if (b.x > 1 - R) { b.x = 1 - R; hit (b, 'r', b.y, std::abs (b.vx)); b.vx = -std::abs (b.vx) * bounce; }
            if (b.y < R)     { b.y = R;     hit (b, 't', b.x, std::abs (b.vy)); b.vy =  std::abs (b.vy) * bounce; }
            if (b.y > 1 - R) { b.y = 1 - R; hit (b, 'b', b.x, std::abs (b.vy)); b.vy = -std::abs (b.vy) * bounce; }
        }

        if (++trailTick >= Ball::trailEvery)
        {
            trailTick = 0;
            for (int i = 0; i < count; ++i)
                balls[(size_t) i].recordTrail();
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
    static constexpr float  gravity = 2.6f, shakeAmp = 0.08f, followTravel = 0.5f, followSway = 0.1f, maxSpeed = 40;
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
