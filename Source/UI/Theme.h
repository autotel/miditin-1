#pragma once
#include <juce_graphics/juce_graphics.h>

/** The v2 prototype's palette, read out of its `COL` table and inline styles. Warm paper ground,
    near-black ink, one orange accent, one blue for input -- spec §8. */
namespace mdt::theme
{
inline const juce::Colour ground   { 0xffe4e0d6 };   // behind the plugin window
inline const juce::Colour paper    { 0xfffbfaf6 };   // the window, cards, white keys
inline const juce::Colour shade    { 0xffefebe2 };   // energy disc, alternate wheel slices
inline const juce::Colour ink      { 0xff1d1c1a };
inline const juce::Colour muted    { 0xff8d887e };
inline const juce::Colour line     { 0xffe2ddd2 };   // faint guides
inline const juce::Colour line2    { 0xffc4beb1 };   // stronger guides, unfiltered energy
inline const juce::Colour accent   { 0xffdc6a48 };   // orange: transmission, pulses, output
inline const juce::Colour input    { 0xff4e8fd8 };   // blue: held input notes
inline const juce::Colour pillTrack{ 0xffe8e3d8 };   // segmented-control track
inline const juce::Colour pillText { 0xff6e695f };   // unselected segment label
inline const juce::Colour logEdge  { 0xffcfc9bd };   // recent-notes chips
inline const juce::Colour field    { 0xffffffff };   // text entry

inline juce::Colour accentA (float a) { return accent.withAlpha (juce::jlimit (0.0f, 1.0f, a)); }
} // namespace mdt::theme
