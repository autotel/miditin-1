#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Fonts.h"
#include "Host.h"

/** The prototype's three control shapes, drawn rather than skinned: a pill-shaped segmented
    control, a bordered toggle button, and a labelled range slider. */
namespace mdt
{
/** A rounded track with one segment selected (ink fill, paper text). Wraps into rows when given
    fewer columns than options. */
class Segmented : public juce::Component
{
public:
    Segmented (juce::StringArray labels, int columns, const fonts::Role& role = fonts::button);

    void setSelected (int index);
    int  getSelected() const noexcept { return selected; }
    void setLabels (const juce::StringArray&);
    std::function<void (int)> onChange;

    /** Height for this many rows at the prototype's padding. */
    static int heightFor (int rows, int padY);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int padY = 4;

private:
    juce::StringArray labels;
    int columns, selected = 0;
    fonts::Role role;
    juce::Rectangle<float> cell (int i) const;
};

/** A 1 px ink box, ink-filled when on. */
class BoxButton : public juce::Component
{
public:
    explicit BoxButton (juce::String t, const fonts::Role& r = fonts::button) : text (std::move (t)), role (r) {}

    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool on) { if (on != lit) { lit = on; repaint(); } }
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    juce::String text;
    fonts::Role role;
    bool lit = false;
};

/** LABEL | track | readout, like the prototype's <label><input type=range>. Bound to a host
    parameter, or to a getter/setter pair for patch values (the CC number). */
class RangeRow : public juce::Component
{
public:
    RangeRow (Host&, P param, int labelWidth = 42, int valueWidth = 36);
    RangeRow (juce::String label, float min, float max, float step,
              std::function<float()> get, std::function<void (float)> set,
              std::function<juce::String (float)> show, int labelWidth = 42, int valueWidth = 36);

    void setLabel (const juce::String& l) { if (l != label) { label = l; repaint(); } }
    /** Re-read the value; called from the editor's frame timer so automation shows. */
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    /** Where the readout sits: right-aligned after the track, or left-aligned (header / gate). */
    bool valueLeft = false;
    float stepOverride = -1;

private:
    juce::String label;
    float min, max, step, def;
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<juce::String (float)> show;
    std::function<void()> begin, end;
    int labelWidth, valueWidth;
    float shown = 0;

    juce::Rectangle<float> track() const;
    void setFromX (float x);
};
} // namespace mdt
