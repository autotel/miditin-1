#include "Controls.h"
#include "Theme.h"

namespace mdt
{
// -- Segmented ---------------------------------------------------------------------------------

Segmented::Segmented (juce::StringArray l, int cols, const fonts::Role& r)
    : labels (std::move (l)), columns (cols), role (r)
{
}

void Segmented::setSelected (int i)
{
    if (i != selected) { selected = i; repaint(); }
}

void Segmented::setLabels (const juce::StringArray& l)
{
    if (l != labels) { labels = l; repaint(); }
}

int Segmented::heightFor (int rows, int padY)
{
    // CSS: 9 px text at line-height normal (~12 px) + 2 x padding per row, 2 px track padding.
    return rows * (12 + 2 * padY) + (rows - 1) * 2 + 4;
}

juce::Rectangle<float> Segmented::cell (int i) const
{
    const int rows = (labels.size() + columns - 1) / columns;
    const auto inner = getLocalBounds().toFloat().reduced (2);
    const float w = inner.getWidth() / (float) columns;
    const float h = (inner.getHeight() - (float) (rows - 1) * 2) / (float) rows;
    return { inner.getX() + (float) (i % columns) * w, inner.getY() + (float) (i / columns) * (h + 2), w, h };
}

void Segmented::paint (juce::Graphics& g)
{
    const int rows = (labels.size() + columns - 1) / columns;
    const auto b = getLocalBounds().toFloat();
    g.setColour (theme::pillTrack);
    g.fillRoundedRectangle (b, rows > 1 ? 9.0f : b.getHeight() * 0.5f);

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto c = cell (i);
        const bool on = i == selected;
        if (on)
        {
            g.setColour (theme::ink);
            g.fillRoundedRectangle (c, c.getHeight() * 0.5f);
        }
        g.setColour (on ? theme::paper : theme::pillText);
        fonts::drawTracked (g, labels[i], c, role, juce::Justification::centred);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (e.position))
        {
            setSelected (i);
            if (onChange) onChange (i);
            return;
        }
}

// -- BoxButton ---------------------------------------------------------------------------------

void BoxButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (lit ? theme::ink : theme::paper);
    g.fillRect (b);
    g.setColour (theme::ink);
    g.drawRect (b, 1.0f);
    g.setColour (lit ? theme::paper : theme::ink);
    g.setFont (fonts::font (role));
    g.drawText (text, b.reduced (6, 0), juce::Justification::centred, true);   // a long preset name ellipsises
}

// -- RangeRow ----------------------------------------------------------------------------------

RangeRow::RangeRow (Host& host, P p, int lw, int vw)
    : label (spec (p).label), min (spec (p).min), max (spec (p).max), step (spec (p).step), def (spec (p).def),
      get ([&host, p] { return host.param (p); }),
      set ([&host, p] (float v) { host.setParam (p, v); }),
      show ([p] (float v) { return juce::String (juce::CharPointer_UTF8 (format (p, v).c_str())); }),
      begin ([&host, p] { host.beginGesture (p); }),
      end ([&host, p] { host.endGesture (p); }),
      labelWidth (lw), valueWidth (vw)
{
    refresh();
}

RangeRow::RangeRow (juce::String l, float lo, float hi, float st,
                    std::function<float()> g, std::function<void (float)> s,
                    std::function<juce::String (float)> sh, int lw, int vw)
    : label (std::move (l)), min (lo), max (hi), step (st), def (lo), get (std::move (g)), set (std::move (s)),
      show (std::move (sh)), labelWidth (lw), valueWidth (vw)
{
    def = get();
    refresh();
}

void RangeRow::refresh()
{
    const float v = get();
    if (std::abs (v - shown) > 1e-6f) { shown = v; repaint(); }
}

juce::Rectangle<float> RangeRow::track() const
{
    auto b = getLocalBounds().toFloat();
    b.removeFromLeft ((float) labelWidth + (labelWidth > 0 ? 6.0f : 0.0f));
    b.removeFromRight ((float) valueWidth + 6.0f);
    return b;
}

void RangeRow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (labelWidth > 0)
    {
        g.setColour (theme::muted);
        fonts::drawTracked (g, label, b.withWidth ((float) labelWidth), fonts::label, juce::Justification::centredLeft);
    }

    // A browser range input with `accent-color: ink`: a thin rounded track, the filled part ink,
    // a round thumb.
    const auto t = track();
    const float cy = t.getCentreY(), r = 5.0f;
    const float x0 = t.getX() + r, x1 = t.getRight() - r;
    const float frac = juce::jlimit (0.0f, 1.0f, (shown - min) / (max - min));
    const float x = x0 + frac * (x1 - x0);
    g.setColour (theme::line2);
    g.fillRoundedRectangle (x0 - 2, cy - 2, x1 - x0 + 4, 4, 2);
    g.setColour (theme::ink);
    g.fillRoundedRectangle (x0 - 2, cy - 2, x - x0 + 2, 4, 2);
    g.fillEllipse (x - r, cy - r, 2 * r, 2 * r);

    g.setColour (theme::ink);
    const auto vb = b.withLeft (b.getRight() - (float) valueWidth);
    fonts::drawTracked (g, show (shown), vb, fonts::label,
                        valueLeft ? juce::Justification::centredLeft : juce::Justification::centredRight);
}

void RangeRow::setFromX (float x)
{
    const auto t = track().reduced (5, 0);
    float v = min + juce::jlimit (0.0f, 1.0f, (x - t.getX()) / t.getWidth()) * (max - min);
    const float st = stepOverride > 0 ? stepOverride : step;
    if (st > 0)
        v = min + std::round ((v - min) / st) * st;
    set (juce::jlimit (min, max, v));
    refresh();
}

void RangeRow::mouseDown (const juce::MouseEvent& e)
{
    if (begin) begin();
    setFromX (e.position.x);
}

void RangeRow::mouseDrag (const juce::MouseEvent& e) { setFromX (e.position.x); }
void RangeRow::mouseUp (const juce::MouseEvent&) { if (end) end(); }

void RangeRow::mouseDoubleClick (const juce::MouseEvent&)
{
    if (begin) begin();
    set (def);
    if (end) end();
    refresh();
}
} // namespace mdt
