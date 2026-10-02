#include "Fonts.h"
#include <BinaryData.h>
#include <array>

namespace mdt::fonts
{
namespace
{
    struct Bundled { Family family; int weight; const char* data; int size; };

    const std::array<Bundled, 5> bundled { {
        { Family::mono,      400, BinaryData::IBMPlexMonoRegular_ttf,           BinaryData::IBMPlexMonoRegular_ttfSize },
        { Family::mono,      500, BinaryData::IBMPlexMonoMedium_ttf,            BinaryData::IBMPlexMonoMedium_ttfSize },
        { Family::mono,      600, BinaryData::IBMPlexMonoSemiBold_ttf,          BinaryData::IBMPlexMonoSemiBold_ttfSize },
        { Family::condensed, 500, BinaryData::IBMPlexSansCondensedMedium_ttf,   BinaryData::IBMPlexSansCondensedMedium_ttfSize },
        { Family::condensed, 600, BinaryData::IBMPlexSansCondensedSemiBold_ttf, BinaryData::IBMPlexSansCondensedSemiBold_ttfSize },
    } };

    /** Parsed once per file and kept: creating a typeface parses the whole TTF. */
    juce::Typeface::Ptr typefaceFor (Family family, int weight)
    {
        static std::array<juce::Typeface::Ptr, bundled.size()> cache;

        size_t best = 0;
        int bestDistance = std::numeric_limits<int>::max();
        for (size_t i = 0; i < bundled.size(); ++i)
            if (bundled[i].family == family && std::abs (bundled[i].weight - weight) < bestDistance)
            {
                bestDistance = std::abs (bundled[i].weight - weight);
                best = i;
            }

        if (cache[best] == nullptr)
            cache[best] = juce::Typeface::createSystemTypefaceFor (bundled[best].data, (size_t) bundled[best].size);
        return cache[best];
    }
} // namespace

juce::Font get (Family family, int weight, float heightPx)
{
    // CSS `font-size` is the em size, which JUCE calls the point height (its plain height is
    // ascent + descent, ~1.3 em for Plex).
    return juce::Font (juce::FontOptions (typefaceFor (family, weight)).withPointHeight (heightPx));
}

float trackedWidth (const juce::String& s, const Role& r)
{
    const auto f = font (r);
    if (r.tracking <= 0.0f)
        return juce::GlyphArrangement::getStringWidth (f, s);

    float w = 0.0f;
    for (auto c : s)
        w += juce::GlyphArrangement::getStringWidth (f, juce::String::charToString (c)) + r.tracking * r.height;
    return w;
}

void drawTracked (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> area,
                  const Role& r, juce::Justification justification)
{
    const auto f = font (r);
    g.setFont (f);
    if (r.tracking <= 0.0f)
    {
        g.drawText (s, area, justification, false);
        return;
    }

    const float extra = r.tracking * r.height;
    const float total = trackedWidth (s, r);
    float x = area.getX();
    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = area.getRight() - total;

    for (auto c : s)
    {
        const auto one = juce::String::charToString (c);
        const float w = juce::GlyphArrangement::getStringWidth (f, one);
        g.drawText (one, juce::Rectangle<float> (x, area.getY(), w + 1.0f, area.getHeight()),
                    juce::Justification (juce::Justification::left | justification.getOnlyVerticalFlags()), false);
        x += w + extra;
    }
}

void text (juce::Graphics& g, const juce::String& s, float x, float baseline, const Role& r,
           juce::Colour c, juce::Justification j)
{
    const auto f = font (r);
    g.setFont (f);
    g.setColour (c);
    float w = juce::GlyphArrangement::getStringWidth (f, s);
    if (j.testFlags (juce::Justification::horizontallyCentred)) x -= w * 0.5f;
    else if (j.testFlags (juce::Justification::right))          x -= w;
    g.drawSingleLineText (s, juce::roundToInt (x), juce::roundToInt (baseline));
}
} // namespace mdt::fonts
