#include "Views.h"
#include "Fonts.h"
#include "Theme.h"

namespace mdt::views
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float tau = juce::MathConstants<float>::twoPi;
    const char* const noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    int mod12 (int m) { return ((m % 12) + 12) % 12; }

    /** The prototype's `txt()`: Plex Mono at a baseline, default 9 px regular. */
    void txt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c,
              float size = 9, int weight = 400, juce::Justification j = juce::Justification::left)
    {
        fonts::text (g, s, x, y, fonts::Role { fonts::Family::mono, weight, size, 0 }, c, j);
    }

    juce::Colour inkA (float a) { return theme::ink.withAlpha (a); }

    void disc (juce::Graphics& g, juce::Point<float> p, float r) { g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r); }
    void ring (juce::Graphics& g, juce::Point<float> p, float r, float w) { g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, w); }

    juce::Point<float> polar (float cx, float cy, float r, float a) { return { cx + std::cos (a) * r, cy + std::sin (a) * r }; }

    /** Canvas-style arc: angle 0 at 3 o'clock, increasing clockwise. */
    juce::Path arc (float cx, float cy, float r, float a0, float a1)
    {
        juce::Path p;
        const int n = std::max (2, (int) (std::abs (a1 - a0) * r / 2));
        for (int i = 0; i <= n; ++i)
        {
            const auto q = polar (cx, cy, r, a0 + (a1 - a0) * (float) i / (float) n);
            i ? p.lineTo (q) : p.startNewSubPath (q);
        }
        return p;
    }

    void stroke (juce::Graphics& g, const juce::Path& p, float w)
    {
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void line (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float w)
    {
        juce::Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        stroke (g, p, w);
    }

    /** Canvas `setLineDash` + `lineDashOffset`: JUCE's own dashing has no offset, and the energy
        cable's flowing dashes are nothing but an offset. */
    juce::Path dashed (const juce::Path& src, float on, float off, float offset = 0)
    {
        juce::Path out;
        const float period = on + off;
        float d = std::fmod (std::fmod (offset, period) + period, period);   // phase at the start
        bool open = false;
        for (juce::PathFlatteningIterator it (src, {}, 0.2f); it.next();)
        {
            const juce::Point<float> a (it.x1, it.y1), b (it.x2, it.y2);
            const float len = a.getDistanceFrom (b);
            for (float at = 0; at < len;)
            {
                const bool inOn = d < on;
                const float step = std::min (len - at, inOn ? on - d : period - d);
                if (inOn)
                {
                    if (! open) { out.startNewSubPath (a + (b - a) * (at / len)); open = true; }
                    out.lineTo (a + (b - a) * ((at + step) / len));
                }
                at += step;
                d += step;
                if (d >= on - 1e-4f) open = false;
                if (d >= period - 1e-4f) d = 0;
            }
        }
        return out;
    }

    /** A pie slice, as `moveTo(centre); arc(); closePath()`. */
    juce::Path wedge (float cx, float cy, float r, float a0, float a1)
    {
        juce::Path p;
        p.startNewSubPath (cx, cy);
        const int n = std::max (2, (int) ((a1 - a0) * r / 2));
        for (int i = 0; i <= n; ++i)
            p.lineTo (polar (cx, cy, r, a0 + (a1 - a0) * (float) i / (float) n));
        p.closeSubPath();
        return p;
    }

    juce::Path segment (juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        return p;
    }
}

juce::String noteName (int m) { return juce::String (noteNames[mod12 (m)]) + juce::String ((int) std::floor (m / 12.0) - 1); }
juce::String signedTone (int d) { return d > 0 ? "+" + juce::String (d) : juce::String (d); }

// -- shared geometry ---------------------------------------------------------------------------------

P EnergyGeo::arcParam (const Patch& p)
{
    return p.enSource == EnergySource::audio ? P::enGain
         : p.enSource != EnergySource::osc   ? P::count
         : p.enSync ? P::enDiv : P::enRate;
}

Pool poolFor (const Patch& p, const ParamValues& pv, const UiSnapshot& s)
{
    TransformConfig c;
    c.mode = p.tfMode;
    c.root = p.root;
    c.scale = p.scale;
    c.pcs = p.pcs;
    c.octave = pv[P::tfOctave];
    c.period = pv.i (P::tfOctLen);
    c.wrap = p.wrap;
    return Pool::make (c, s.held.data(), s.heldCount);
}

WheelGeo WheelGeo::make (const Patch& p, const ParamValues& pv, const UiSnapshot& s)
{
    WheelGeo g;
    if (p.tfMode == TransformMode::held)
        g.lo = s.heldCount > 0 ? (int) std::floor (*std::min_element (s.held.begin(), s.held.begin() + s.heldCount) / 12.0) * 12 : 48;
    else
        g.lo = ((int) std::floor (pv[P::tfOctave]) + 1) * 12;
    const bool w = p.wrap;
    g.hi = g.lo + (w ? 35 : 11);
    g.cx = CW / 2;
    g.cy = CH / 2 + 3;
    g.r0 = w ? 12.0f : 38.0f;
    g.r1 = w ? 44.0f : 38.0f;
    g.rl = w ? 53.0f : 50.0f;
    g.span = w ? 36.0f : 12.0f;
    return g;
}

float WheelGeo::angle (int m) const { return (float) mod12 (m) / 12.0f * tau - pi / 2; }
float WheelGeo::radius (int m) const { return r0 + (float) (juce::jlimit (lo, hi + 1, m) - lo) / span * (r1 - r0); }
juce::Point<float> WheelGeo::at (int m) const { return polar (cx, cy, radius (m), angle (m)); }

std::vector<Key> keys (int lo, int hi, float width)
{
    int whites = 0;
    for (int m = lo; m <= hi; ++m)
        whites += ! juce::MidiMessage::isMidiNoteBlack (m);
    const float ww = width / (float) whites;
    std::vector<Key> out;
    int wi = 0;
    for (int m = lo; m <= hi; ++m)
    {
        if (! juce::MidiMessage::isMidiNoteBlack (m))
            out.push_back ({ m, (float) wi++ * ww, ww, false });
        else
            out.push_back ({ m, (float) wi * ww - ww * 0.32f, ww * 0.64f, true });
    }
    return out;
}

Anchors anchorsFor (const Frame& f)
{
    Anchors a;
    const EnergyGeo G;
    a.lfo = { G.cx + G.rm, G.y (f.s->views[0].energy) };

    switch (f.patch->pulse)
    {
        case PulseKind::wheel:   a.pTop = { CW / 2 - 12, CH / 2 - 2 }; a.pOut = { CW / 2 - 12 + CH / 2 - 12 + 13, CH / 2 - 2 }; break;
        case PulseKind::shaker:  a.pTop = { CW / 2, 2 + (CH - 4) / 2 }; a.pOut = { (CW + CH - 4) / 2, 2 + (CH - 4) / 2 }; break;
        case PulseKind::digital: a.pTop = { (30 + CW - 4) / 2, 14 + (CH - 14) / 5 * 2.5f }; a.pOut = { CW, 14 + (CH - 14) / 5 * 2.5f }; break;
        // No energy cable: energy drives nothing here. The wire leaves the circle's edge.
        case PulseKind::midiIn:  a.pOut = { CW / 2 + 26, CH / 2 + 4 }; break;
    }
    // Cables end where a drawing has a centre to reach (wheel hub, box, arp ring, tone wheel), and
    // at the edge where it has none (the MIDI-in label, the melody grid).

    if (f.patch->gen == GenKind::melody)
    {
        a.gC = { 0, CH / 2 + 6 };
        a.gOut = { CW, CH / 2 + 6 };
    }
    else
    {
        const int n = f.s->seq.n;
        const float R = n > 1 ? CH / 2 - 14 : 0, nr = n > 8 ? 7.0f : 10.0f;
        a.gC = { CW / 2, CH / 2 + 2 };
        a.gOut = { CW / 2 + (n ? R + nr + 4 : 0), CH / 2 + 2 };
    }
    a.tC = { CW / 2, CH / 2 + 3 };
    return a;
}

// -- ENERGY ------------------------------------------------------------------------------------------

void drawEnergy (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    const auto& p = *f.patch;
    const EnergyGeo G;
    const float cx = G.cx, cy = G.cy, rm = G.rm, top = cy - rm, xl = cx - rm, xr = cx + rm;
    const float center = f.pv[P::enCenter], depth = f.pv[P::enDepth];
    const float b0 = juce::jlimit (0.0f, 1.0f, center - depth / 2), b1 = juce::jlimit (0.0f, 1.0f, center + depth / 2);
    const auto& hover = f.energyHover;

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path disc;
        disc.addEllipse (cx - rm, cy - rm, 2 * rm, 2 * rm);
        g.reduceClipRegion (disc);
        g.setColour (theme::shade);
        g.fillRect (xl, top, 2 * rm, 2 * rm);
        g.setColour (theme::accentA (hover == "mid" ? 0.2f : 0.13f));
        g.fillRect (xl, G.y (b1), 2 * rm, G.y (b0) - G.y (b1));
        g.setColour (theme::accent);
        line (g, { xl, G.y (b1) }, { xr, G.y (b1) }, hover == "top" ? 2.0f : 1.0f);
        line (g, { xl, G.y (b0) }, { xr, G.y (b0) }, hover == "bot" ? 2.0f : 1.0f);
        g.setColour (theme::muted);
        stroke (g, dashed (segment ({ xl, G.y (center) }, { xr, G.y (center) }), 2, 3), 1);
        if (p.enFilter == EnergyFilter::hpf)   // where the HPF's output rests; dragged
        {
            const float yz = G.y (f.pv[P::enHpfZero]);
            g.setColour (theme::accent);
            stroke (g, dashed (segment ({ xl, yz }, { xr, yz }), 5, 3), hover == "zero" ? 2.0f : 1.0f);
        }
    }

    {
        // The plot is clipped by the circle only on its old (left) side; it may run out on the right.
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip = arc (cx, cy, rm, -pi / 2, -3 * pi / 2);
        clip.lineTo (cx, CH);
        clip.lineTo (CW, CH);
        clip.lineTo (CW, 0);
        clip.lineTo (cx, 0);
        clip.closeSubPath();
        g.reduceClipRegion (clip);

        auto plot = [&] (const std::array<float, historyLength>& h, int n) {
            juce::Path path;
            const float st = 2 * rm / (float) historyLength;
            for (int i = 0; i < n; ++i)
            {
                const juce::Point<float> q { xr - (float) (n - 1 - i) * st, G.y (h[(size_t) i]) };
                i ? path.lineTo (q) : path.startNewSubPath (q);
            }
            return path;
        };
        if (f.numHistories > 0 && p.enFilter != EnergyFilter::off)
        {
            g.setColour (theme::line2);
            stroke (g, dashed (plot ((*f.histories)[0].raw, (*f.histories)[0].count), 2, 2), 1);
        }
        for (int i = f.numHistories - 1; i >= 0; --i)
        {
            g.setColour (i ? inkA (0.45f) : theme::ink);
            stroke (g, plot ((*f.histories)[(size_t) i].val, (*f.histories)[(size_t) i].count), i ? 1.1f : 1.5f);
        }
    }

    for (int i = s.numViews - 1; i >= 0; --i)
    {
        g.setColour (i ? theme::accentA (0.6f) : theme::accent);
        disc (g, { xr, G.y (s.views[(size_t) i].energy) }, i ? 2.5f : 3.5f);
    }

    if (const auto ap = EnergyGeo::arcParam (p); ap != P::count)
    {
        const float ra = rm + 6, a0 = -pi / 2 - rateArc, a1 = -pi / 2 + rateArc;
        const float n = EnergyGeo::arcN (p, f.pv), aN = a0 + (a1 - a0) * n;
        const bool hot = hover == "rate";
        g.setColour (theme::line2);
        stroke (g, arc (cx, cy, ra, a0, a1), 1.5f);
        g.setColour (theme::ink);
        stroke (g, arc (cx, cy, ra, a0, aN), hot ? 3.0f : 2.0f);
        if (ap == P::enDiv)
        {
            g.setColour (theme::muted);
            for (int i = 0; i < 8; ++i)
                disc (g, polar (cx, cy, ra + 4, a0 + (a1 - a0) * (float) i / 7.0f), 0.9f);
        }
        g.setColour (theme::ink);
        disc (g, polar (cx, cy, ra, aN), hot ? 4.0f : 3.0f);
        const auto label = juce::String (juce::CharPointer_UTF8 (format (ap, f.pv[ap]).c_str()));
        const auto lp = polar (cx, cy, ra, a1);
        txt (g, label, lp.x + 6, lp.y + 2, hot ? theme::ink : theme::muted, 8);
    }

    const float val = s.views[0].energy;
    txt (g, juce::String (val, 2) + (s.numViews > 1 ? fonts::utf8 ("  \xc3\x97") + juce::String (s.numViews) : juce::String()),
         0, CH - 2, theme::ink, 8, 600);
    juce::String src = p.enSource == EnergySource::osc ? "" : p.enSource == EnergySource::vel ? "in vel"
                     : p.enSource == EnergySource::note ? "in note" : p.enSource == EnergySource::audio ? "audio"
                     : "cc" + juce::String (p.ccNumber);
    if (p.enFilter != EnergyFilter::off)
        src += juce::String (p.enFilter == EnergyFilter::lpf ? " lpf" : " hpf")
             + (p.enEdge == EnergyEdge::pos ? "+" : p.enEdge == EnergyEdge::neg ? "-" : "");
    txt (g, src, CW, CH - 2, theme::muted, 8, 400, juce::Justification::right);
}

// -- PULSE -------------------------------------------------------------------------------------------

namespace
{
void drawWheel (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    const int slices = f.pv.i (P::whSlices);
    const auto& wh = s.views[0];
    const float cx = CW / 2 - 12, cy = CH / 2 - 2, R = CH / 2 - 12, stp = tau / (float) slices;
    const float a = (float) std::fmod (wh.wheelA, (double) tau);

    for (int i = 0; i < slices; ++i)
    {
        const auto w2 = wedge (cx, cy, R, a + (float) i * stp - pi / 2, a + (float) (i + 1) * stp - pi / 2);
        g.setColour (i % 2 ? theme::shade : theme::paper);
        g.fillPath (w2);
        g.setColour (theme::ink);
        stroke (g, w2, 1);
    }
    g.setColour (theme::ink);
    ring (g, { cx, cy }, R, 1.5f);
    for (int i = 0; i < slices; ++i)
        disc (g, polar (cx, cy, R - 4, a + (float) i * stp - pi / 2), 2);
    g.setColour (theme::paper);
    disc (g, { cx, cy }, 7);
    g.setColour (theme::ink);
    ring (g, { cx, cy }, 7, 1.5f);
    disc (g, { cx, cy }, 2);

    for (int v = 1; v < s.numViews; ++v)
    {
        const float av = (float) std::fmod (s.views[(size_t) v].wheelA, (double) tau);
        for (int i = 0; i < slices; ++i)
        {
            const float ai = av + (float) i * stp - pi / 2;
            g.setColour (inkA (0.4f));
            line (g, polar (cx, cy, 9, ai), polar (cx, cy, R, ai), 1);
            g.setColour (theme::accentA (0.75f));
            disc (g, polar (cx, cy, R - 4, ai), 1.8f);
        }
    }

    float fk = 0;
    for (int v = 0; v < s.numViews; ++v)
        fk = std::max (fk, s.views[(size_t) v].flick);
    juce::Path pointer;
    pointer.addTriangle (0, -5, 0, 5, -15, 0);
    pointer.applyTransform (juce::AffineTransform::rotation (-fk * 0.55f).translated (cx + R + 13, cy));
    g.setColour (f.dk (s.pulseT, 0.12) > 0 ? theme::accent : theme::ink);
    g.fillPath (pointer);

    const double revs = wh.wheelW / (double) tau;
    txt (g, juce::String (revs, 2) + " rev/s" + (s.numViews > 1 ? fonts::utf8 ("  \xc3\x97") + juce::String (s.numViews) : juce::String()),
         0, CH - 1, theme::ink, 8, 600);
    txt (g, juce::String (revs * slices, 1) + " p/s", CW, CH - 1, theme::muted, 8, 400, juce::Justification::right);
}

void drawShaker (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    // The box drawn where the energy moved it, scaled down to a few px so it stays in the canvas.
    const auto off = [] (float d, float gain) { return juce::jlimit (-6.0f, 6.0f, d * gain); };
    const float S = CH - 4, bx = (CW - S) / 2 + off (s.views[0].boxX, 40), by = 2 + off (s.views[0].boxY, 16), R = Shaker::radius;
    g.setColour (theme::ink);
    g.drawRect (bx, by, S, S, 1.5f);

    for (int v = 0; v < s.numViews; ++v)
        for (const auto& w : s.views[(size_t) v].walls)
        {
            const float a = 1.0f - (float) (s.t - w.t) / 0.4f;
            if (a <= 0 || a > 1 || w.wall == 0) continue;
            const float L = 14 + w.speed * 8;
            g.setColour (theme::accentA (a));
            if (w.wall == 'l' || w.wall == 'r')
            {
                const float X = w.wall == 'l' ? bx : bx + S, Y = by + w.pos * S, d = w.wall == 'l' ? -1.0f : 1.0f;
                line (g, { X, Y - L / 2 }, { X, Y + L / 2 }, 3.5f);
                for (float o : { -5.0f, 0.0f, 5.0f })
                    line (g, { X + d * 3, Y + o }, { X + d * (4 + 6 * (1 - a)), Y + o * 1.4f }, 1);
            }
            else
            {
                const float Y = w.wall == 't' ? by : by + S, X = bx + w.pos * S, d = w.wall == 't' ? -1.0f : 1.0f;
                line (g, { X - L / 2, Y }, { X + L / 2, Y }, 3.5f);
                for (float o : { -5.0f, 0.0f, 5.0f })
                    line (g, { X + o, Y + d * 3 }, { X + o * 1.4f, Y + d * (4 + 6 * (1 - a)) }, 1);
            }
        }

    for (int v = 0; v < s.numViews; ++v)
        for (int i = 0; i < s.views[(size_t) v].ballCount; ++i)
        {
            const auto& b = s.views[(size_t) v].balls[(size_t) i];
            const float hf = std::max (0.0f, 1.0f - (float) (s.t - b.hit) / 0.25f);
            const juce::Point<float> q { bx + b.x * S, by + b.y * S };
            // The path it actually took over the last 150 ms, fading with age.
            auto prev = q;
            for (int k = 0; k < b.trailCount; ++k)
            {
                const juce::Point<float> p { bx + b.tx[(size_t) k] * S, by + b.ty[(size_t) k] * S };
                g.setColour (theme::muted.withMultipliedAlpha (0.7f * (1.0f - (float) k / Ball::trailLength)));
                line (g, prev, p, 1);
                prev = p;
            }
            g.setColour (hf > 0 ? theme::accent : (v ? theme::muted : theme::ink));
            disc (g, q, R * S);
        }
    txt (g, juce::String (s.views[0].effort, 2), bx + 4, by + 11, theme::muted, 8);
}

void drawDigital (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    const int base = f.patch->digBase;
    const double DB = s.views[0].beat;
    const float x0 = 30, x1 = CW - 4, top = 14, rh = (CH - top) / 5;
    const double w0 = std::floor (DB / 4) * 4;
    auto X = [&] (double b) { return x0 + (float) ((b - w0) / 4) * (x1 - x0); };

    g.setColour (theme::paper);
    g.fillRect (0.0f, 0.0f, CW, CH);
    for (int r = 0; r < 5; ++r)
    {
        const float yc = top + (float) r * rh + rh / 2;
        bool act = false;
        float flash = 0;
        for (int v = 0; v < s.numViews; ++v)
            if (s.views[(size_t) v].digDiv == r)
            {
                act = true;
                flash = std::max (flash, f.dk (s.views[(size_t) v].digT, 0.15));
            }
        if (act)
        {
            g.setColour (theme::accentA (0.1f + 0.25f * flash));
            g.fillRect (0.0f, yc - rh / 2 + 1, CW, rh - 2);
        }
        txt (g, "1/" + juce::String (base * (1 << r)), 0, yc + 3, act ? theme::ink : theme::muted, 8, act ? 600 : 400);
        g.setColour (act ? theme::ink : theme::line2);
        line (g, { x0, yc + 0.5f }, { x1, yc + 0.5f }, 1);
        const double d = Digital::cell (base, r);
        for (int j = 0; (double) j * d < 4 - 1e-6; ++j)
        {
            const double b = w0 + j * d;
            const bool passed = b <= DB;
            const bool big = std::abs (b - std::round (b)) < 1e-6;
            g.setColour (act ? (passed ? theme::ink : theme::line2) : theme::line2);
            const float px = X (b);
            line (g, { px, yc - (big ? 7.0f : 4.0f) }, { px, yc + (big ? 7.0f : 4.0f) }, act ? 2.0f : 1.0f);
        }
    }
    g.setColour (theme::accent);
    line (g, { X (DB), top - 2 }, { X (DB), CH }, 1.5f);
    txt (g, "grid 1/" + juce::String (base * (1 << s.views[0].digDiv)), 0, 9, theme::ink, 9, 600);
    txt (g, "beat " + juce::String ((int) std::floor (DB) % 4 + 1), CW, 9, theme::muted, 9, 400, juce::Justification::right);
}

void drawMidiIn (juce::Graphics& g, const Frame& f)
{
    // Under CLOCK the flash is the pulse, not the note.
    static const char* const divNames[] { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const bool clock = f.patch->midiInSource == MidiInSource::clock;
    const float a = f.dk (clock ? f.s->pulseT : f.s->inT, 0.3), cx = CW / 2, cy = CH / 2 + 4;
    g.setColour (theme::ink);
    ring (g, { cx, cy }, 26, 1.5f);
    g.setColour (theme::accentA (a));
    disc (g, { cx, cy }, 26 * (0.4f + 0.6f * a));
    ring (g, { cx, cy }, 26 + 24 * (1 - a), 1);
    if (clock)
    {
        txt (g, "CLOCK", cx, cy - 1, theme::ink, 9, 600, juce::Justification::horizontallyCentred);
        txt (g, divNames[std::clamp (f.patch->clockDiv, 0, 5)], cx, cy + 10, theme::muted, 8, 400, juce::Justification::horizontallyCentred);
    }
    else
    {
        txt (g, "MIDI IN", cx, cy + 3, theme::ink, 9, 600, juce::Justification::horizontallyCentred);
    }
    g.setColour (theme::ink);
    stroke (g, dashed (segment ({ cx, CH }, { cx, cy + 30 }), 3, 3), 1);
}
} // namespace

void drawPulse (juce::Graphics& g, const Frame& f)
{
    switch (f.patch->pulse)
    {
        case PulseKind::wheel:   drawWheel (g, f); break;
        case PulseKind::shaker:  drawShaker (g, f); break;
        case PulseKind::digital: drawDigital (g, f); break;
        case PulseKind::midiIn:  drawMidiIn (g, f); break;
    }
}

// -- GENERATE ----------------------------------------------------------------------------------------

void drawGenerate (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    const auto& p = *f.patch;
    const float gf = f.dk (s.genT, 0.2);

    if (p.gen != GenKind::melody)
    {
        const int n = s.seq.n;
        const float cx = CW / 2, cy = CH / 2 + 2, R = n > 1 ? CH / 2 - 14 : 0;
        auto P = [&] (int i) { return polar (cx, cy, R, -pi / 2 + (float) i * tau / (float) n); };
        if (n == 0)
        {
            txt (g, p.gen == GenKind::input ? "hold notes on IN" : "type tones, e.g. 0 2 4", CW / 2, CH / 2,
                 theme::muted, 9, 400, juce::Justification::horizontallyCentred);
            return;
        }
        if (p.order == Order::random)
        {
            g.setColour (theme::line);
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    line (g, P (i), P (j), 1);
        }
        else
        {
            juce::Path ringPath;
            for (int i = 0; i <= n; ++i)
                i ? ringPath.lineTo (P (i % n)) : ringPath.startNewSubPath (P (0));
            g.setColour (theme::line2);
            stroke (g, ringPath, 1);
        }

        const int cur = s.views[0].arpI >= 0 ? s.views[0].arpI % n : -1, prv = s.views[0].arpPrev;
        if (cur >= 0 && prv >= 0 && prv < n && prv != cur)
        {
            const auto a = P (prv), b = P (cur);
            const float k = std::min (1.0f, (1 - gf) * 3 + 0.3f);
            g.setColour (theme::accentA (0.3f + 0.7f * gf));
            line (g, a, a + (b - a) * k, 3);
        }

        const float nr = n > 8 ? 7.0f : 10.0f;
        for (int i = 0; i < n; ++i)
        {
            bool on = false;
            for (int v = 0; v < s.numViews; ++v)
                on = on || (s.views[(size_t) v].arpI >= 0 && s.views[(size_t) v].arpI % n == i);
            const auto q = P (i);
            const float r = on ? nr + 3 * gf : nr;
            g.setColour (on ? theme::accent : theme::paper);
            disc (g, q, r);
            g.setColour (theme::ink);
            ring (g, q, r, 1.2f);
            const int v = s.seq.v[(size_t) i];
            txt (g, p.gen == GenKind::input ? juce::String (v + 1) : signedTone (v), q.x, q.y + 3,
                 on ? theme::paper : theme::ink, n > 8 ? 7.0f : 9.0f, 600, juce::Justification::horizontallyCentred);
        }
        static const char* const orderNames[] { "up", "down", "up/dn", "random", "written" };
        txt (g, orderNames[(int) p.order], cx, cy + 3, theme::muted, 8, 400, juce::Justification::horizontallyCentred);
        if (s.hasDeg)
            txt (g, "tone " + signedTone (s.curDeg), CW, 9, theme::muted, 9, 400, juce::Justification::right);
        return;
    }
    const int len = f.pv.i (P::melLength);
    const float top = 13, GH = CH - top, cw = CW / (float) len, ch = GH / 15;
    auto yy = [&] (int d) { return top + ((float) (7 - d) + 0.5f) * ch; };
    auto xx = [&] (int i) { return ((float) i + 0.5f) * cw; };

    for (int r = 0; r < 15; ++r)
    {
        g.setColour (7 - r == 0 ? theme::line2 : theme::line);
        const float y = std::round (yy (7 - r)) + 0.5f;
        line (g, { 0, y }, { CW, y }, 1);
    }
    for (int i = 0; i <= len; ++i)
    {
        g.setColour (i % 4 == 0 ? theme::line2 : theme::line);
        const float x = std::round ((float) i * cw) + 0.5f;
        line (g, { x, top }, { x, CH }, 1);
    }

    const int cur = s.views[0].melI;
    struct Note { int s, e, d; };
    std::vector<Note> notes;
    for (int i = 0; i < len; ++i)
    {
        const auto v = p.melody[(size_t) i];
        if (v == melTie || v == melRest) continue;
        int j = i + 1;
        while (j < len && p.melody[(size_t) j] == melTie) ++j;
        notes.push_back ({ i, j, v });
    }
    const float bh = std::max (4.0f, ch - 2);

    for (int v = 1; v < s.numViews; ++v)
        if (const int mi = s.views[(size_t) v].melI; mi >= 0 && mi < len)
        {
            g.setColour (theme::accentA (0.14f));
            g.fillRect ((float) mi * cw, top, cw, GH);
        }
    if (cur >= 0 && cur < len)
    {
        g.setColour (theme::accentA (0.08f + 0.22f * gf));
        g.fillRect ((float) cur * cw, top, cw, GH);
    }
    for (size_t k = 0; k + 1 < notes.size(); ++k)
    {
        const auto& A = notes[k];
        const auto& B = notes[k + 1];
        if (A.e != B.s) continue;
        g.setColour (A.e <= cur ? theme::accent : theme::ink);
        line (g, { (float) A.e * cw, yy (A.d) }, { (float) B.s * cw, yy (B.d) }, 1.3f);
    }
    std::vector<bool> covered ((size_t) len, false);
    for (const auto& n : notes)
    {
        for (int i = n.s; i < n.e; ++i) covered[(size_t) i] = true;
        const bool on = cur >= n.s && cur < n.e, played = n.e <= cur;
        const float x0 = (float) n.s * cw + 1, x1 = (float) n.e * cw - 1;
        const float y0 = yy (n.d) - bh / 2 - (on ? gf * 1.5f : 0), hh = bh + (on ? gf * 3 : 0);
        g.setColour (on || played ? theme::accent : theme::ink);
        g.fillRect (x0, y0, x1 - x0, hh);
        if (on && cur > n.s)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (x0, y0, (float) (cur - n.s) * cw, hh);
        }
        g.setColour (theme::paper);
        g.fillRect (x1 - 2, y0 + 1, 1.0f, hh - 2);
    }
    for (int i = 0; i < len; ++i)
        if (! covered[(size_t) i])
        {
            g.setColour (theme::muted);
            g.fillRect (xx (i) - 2, top + GH / 2 - 0.5f, 4.0f, 1.0f);
        }
    txt (g, "step " + juce::String (cur + 1) + "/" + juce::String (len), 0, 9, theme::ink, 9, 600);
    txt (g, s.hasDeg ? "tone " + signedTone (s.curDeg) : juce::String ("rest"), CW, 9, theme::muted, 9, 400, juce::Justification::right);
}

// -- TRANSFORM ---------------------------------------------------------------------------------------

void drawTransform (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    const auto& p = *f.patch;
    const auto G = WheelGeo::make (p, f.pv, s);
    const auto pool = poolFor (p, f.pv, s);

    std::array<bool, 12> pcs {};
    // Absolute pitch classes. The prototype took them relative to the border, which lit the wrong
    // labels whenever a fractional set-mode octave moved the border off a C.
    for (int i = 0; i < pool.n; ++i) pcs[(size_t) mod12 (pool.base + pool.p[(size_t) i])] = true;
    std::array<bool, 128> held {}, setNotes {};
    for (int i = 0; i < s.heldCount; ++i) held[(size_t) s.held[(size_t) i]] = true;
    if (p.tfMode == TransformMode::set)
        for (int d = -40; d <= 80; ++d)
            if (const auto m = pool.transform (d)) setNotes[(size_t) *m] = true;
    auto inPool = [&] (int m) {
        if (p.tfMode == TransformMode::set) return (bool) setNotes[(size_t) m];
        if (p.tfMode == TransformMode::held)
        {
            if (p.wrap) return (bool) held[(size_t) m];
            for (int i = 0; i < s.heldCount; ++i)
                if (mod12 (s.held[(size_t) i] - m) == 0) return true;
            return false;
        }
        return (bool) pcs[(size_t) mod12 (m)];
    };

    g.setColour (theme::line);
    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i / 12.0f * tau - pi / 2;
        line (g, polar (G.cx, G.cy, G.r0 * 0.6f, a), polar (G.cx, G.cy, G.r1 + 3, a), 1);
    }
    juce::Path spiral;
    for (int q = 0; q <= (int) G.span * 4; ++q)
    {
        const float a = ((float) q / 4) / 12 * tau - pi / 2, r = G.r0 + ((float) q / 4) / G.span * (G.r1 - G.r0);
        q ? spiral.lineTo (polar (G.cx, G.cy, r, a)) : spiral.startNewSubPath (polar (G.cx, G.cy, r, a));
    }
    g.setColour (theme::line2);
    stroke (g, spiral, 1);

    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i / 12.0f * tau - pi / 2;
        bool on = pcs[(size_t) i];
        if (p.tfMode == TransformMode::held)
            for (int k = 0; k < s.heldCount; ++k)
                on = on || mod12 (s.held[(size_t) k]) == i;
        const bool root = p.tfMode == TransformMode::scale && i == p.root;
        const auto l = polar (G.cx, G.cy, G.rl, a);
        if (on)
        {
            g.setColour (root ? theme::ink : theme::shade);
            disc (g, l, 6.5f);
        }
        txt (g, noteNames[i], l.x, l.y + 2.5f, root ? theme::paper : (on ? theme::ink : theme::muted), 7, on ? 600 : 400,
             juce::Justification::horizontallyCentred);
    }

    g.setColour (theme::ink);
    for (int m = std::max (0, G.lo); m <= std::min (127, G.hi); ++m)
        if (inPool (m)) disc (g, G.at (m), 2.3f);

    for (const auto& h : s.mappings)
    {
        if (h.note < 0) continue;
        const float a = std::max (0.0f, 1.0f - (float) (s.t - h.t) / 1.2f);
        if (a <= 0 || a > 1) continue;
        const auto q = G.at (h.note);
        g.setColour (theme::accentA (a * 0.8f));
        line (g, { G.cx, G.cy }, q, 1 + 1.5f * a);
        g.setColour (theme::accentA (a));
        disc (g, q, 2.5f + 3 * a);
    }

    if (p.tfMode == TransformMode::set)
    {
        const int bm = pool.base;
        const float a = G.angle (bm), r = G.radius (bm);
        g.setColour (theme::accent);
        line (g, polar (G.cx, G.cy, r - 6, a), polar (G.cx, G.cy, r + 6, a), 1.5f);
    }
    g.setColour (theme::ink);
    disc (g, { G.cx, G.cy }, 1.8f);

    const auto& last = s.mappings[(size_t) ((s.mapNext + 15) % 16)];
    if (last.t > -9)
        txt (g, signedTone (last.deg) + fonts::utf8 (" \xe2\x86\x92 ") + (last.note < 0 ? fonts::utf8 ("\xe2\x80\x93") : noteName (last.note)),
             0, 9, theme::ink, 9, 600);
    else if (pool.n == 0)
        txt (g, "empty", 0, 9, theme::ink, 9, 600);
    txt (g, noteName (G.lo) + fonts::utf8 ("\xe2\x80\x93") + noteName (G.hi), CW, 9, theme::muted, 8, 400, juce::Justification::right);
}

// -- keyboards -------------------------------------------------------------------------------------

namespace
{
template <typename Fill>
void drawKeys (juce::Graphics& g, Fill&& fill)
{
    const auto ks = keys (36, 83, KW);
    for (const auto& k : ks)
    {
        if (k.black) continue;
        g.setColour (theme::paper);
        g.fillRect (k.x, 0.0f, k.w, KH);
        if (const auto c = fill (k.m); c.getAlpha() > 0) { g.setColour (c); g.fillRect (k.x, 0.0f, k.w, KH); }
        g.setColour (theme::ink);
        g.drawRect (k.x + 0.5f, 0.5f, k.w, KH - 1, 1.0f);
        if (k.m % 12 == 0)
            txt (g, noteName (k.m), k.x + k.w / 2, KH - 4, theme::muted, 7, 400, juce::Justification::horizontallyCentred);
    }
    for (const auto& k : ks)
    {
        if (! k.black) continue;
        g.setColour (theme::ink);
        g.fillRect (k.x, 0.0f, k.w, KH * 0.6f);
        if (const auto c = fill (k.m); c.getAlpha() > 0) { g.setColour (c); g.fillRect (k.x + 1, 0.0f, k.w - 2, KH * 0.6f - 1); }
    }
}
} // namespace

void drawInKeys (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    drawKeys (g, [&] (int m) {
        for (int i = 0; i < s.heldCount; ++i)
            if (s.held[(size_t) i] == m) return theme::input;
        return juce::Colours::transparentBlack;
    });
}

void drawOutKeys (juce::Graphics& g, const Frame& f)
{
    const auto& s = *f.s;
    drawKeys (g, [&] (int m) {
        const auto& n = s.notes[(size_t) m];
        if (n.vel == 0) return juce::Colours::transparentBlack;
        const float al = s.t < n.until ? 1.0f : std::max (0.0f, 1.0f - (float) (s.t - n.until) / 0.6f);
        return al > 0 ? theme::accentA (al * (0.3f + 0.7f * (float) n.vel / 127.0f)) : juce::Colours::transparentBlack;
    });
}

// -- connections -----------------------------------------------------------------------------------

void drawConnections (juce::Graphics& g, const Frame& f, const Anchors& a, bool under)
{
    const auto& s = *f.s;
    const bool cableUnder = f.patch->pulse == PulseKind::digital;

    if (under == cableUnder && f.patch->pulse != PulseKind::midiIn)
    {
        const float k = s.energy;
        juce::Path bz;
        bz.startNewSubPath (a.lfo);
        bz.cubicTo (a.lfo + juce::Point<float> (46, 0), a.pTop - juce::Point<float> (60, 0), a.pTop);
        g.setColour (theme::line);
        stroke (g, bz, 1);
        g.setColour (theme::ink);
        stroke (g, dashed (bz, 2 + 6 * k, 6 - 3 * k, -(float) f.flow), 1 + 2.2f * k);
        g.setColour (theme::paper);
        disc (g, a.pTop, 3.5f);
        g.setColour (theme::ink);
        ring (g, a.pTop, 3.5f, 1.2f);
    }
    if (! under)
        return;

    const auto mid = (a.pOut + a.gC) * 0.5f + juce::Point<float> (0, 10 + 6 * f.dk (s.pulseT, 0.2));
    juce::Path wire;
    wire.startNewSubPath (a.pOut);
    wire.quadraticTo (mid, a.gC);
    g.setColour (theme::ink);
    stroke (g, wire, 1);
    if (const float pf = f.dk (s.pulseT, 0.18); pf > 0)
    {
        g.setColour (theme::accentA (pf));
        stroke (g, wire, 1 + 2.5f * s.pulseV * pf);
    }

    g.setColour (theme::muted);
    stroke (g, dashed (segment (a.gOut, a.tC), 1, 4), 1.3f);
    if (const float gf = f.dk (s.genT, 0.22); gf > 0 && s.hasDeg)
    {
        g.setColour (theme::accentA (gf));
        line (g, a.gOut, a.tC, 2);
    }
}
} // namespace mdt::views
