#pragma once
#include <juce_graphics/juce_graphics.h>

/** The prototype's type: IBM Plex Mono for everything, IBM Plex Sans Condensed for the title.
    Both bundled unmodified (SIL OFL 1.1, Reserved Font Name "Plex"; Resources/Fonts/OFL-IBMPlex.txt
    ships with the binary).

    `juce::Font` has no letter-spacing, and the module titles carry 0.18em of it, so
    `drawTracked()` spaces glyphs by hand (ported from mepSynth). */
namespace mdt::fonts
{
enum class Family { mono, condensed };

/** Bundled weights: Mono 400/500/600, Condensed 500/600. Others fall back to the nearest. */
juce::Font get (Family, int weight, float heightPx);

struct Role { Family family; int weight; float height; float tracking; };

inline constexpr Role title       { Family::condensed, 600, 20.0f, -0.01f };
inline constexpr Role caption     { Family::mono,      400,  9.0f,  0.05f };
inline constexpr Role moduleTitle { Family::mono,      600, 10.0f,  0.18f };
inline constexpr Role moduleSub   { Family::mono,      400,  9.0f,  0.0f  };
inline constexpr Role label       { Family::mono,      400,  9.0f,  0.0f  };
inline constexpr Role button      { Family::mono,      400,  9.0f,  0.0f  };
inline constexpr Role buttonSmall { Family::mono,      400,  8.0f,  0.0f  };
inline constexpr Role headerButton{ Family::mono,      400, 10.0f,  0.0f  };
inline constexpr Role canvas      { Family::mono,      400,  8.0f,  0.0f  };
inline constexpr Role canvasBold  { Family::mono,      600,  8.0f,  0.0f  };
inline constexpr Role field       { Family::mono,      400, 11.0f,  0.0f  };

inline juce::Font font (const Role& r) { return get (r.family, r.weight, r.height); }

/** Tracking is in em and, as in CSS, follows the last glyph too. */
void drawTracked (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                  const Role&, juce::Justification);
float trackedWidth (const juce::String& text, const Role&);

/** Canvas-style text at a baseline point, like the prototype's `txt()` helper. */
void text (juce::Graphics&, const juce::String&, float x, float baseline, const Role&,
           juce::Colour, juce::Justification = juce::Justification::left);

/** `juce::String (const char*)` decodes ASCII, not UTF-8; arrows and "×" need this. */
inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
} // namespace mdt::fonts
