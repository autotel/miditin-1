#include "Patch.h"

namespace mdt
{
namespace
{
    // State is written as words, so a saved patch reads in a text editor.
    template <typename E, size_t N>
    void putEnum (juce::XmlElement& x, const char* key, E v, const std::array<const char*, N>& names)
    {
        x.setAttribute (key, names[(size_t) v]);
    }

    template <typename E, size_t N>
    void getEnum (const juce::XmlElement& x, const char* key, E& v, const std::array<const char*, N>& names)
    {
        const auto s = x.getStringAttribute (key);
        for (size_t i = 0; i < N; ++i)
            if (s == names[i])
                v = (E) i;
    }

    const std::array<const char*, 2> voiceNames  { "mono", "poly" };
    const std::array<const char*, 5> sourceNames { "osc", "vel", "note", "cc", "audio" };
    const std::array<const char*, 5> shapeNames  { "sine", "tri", "saw", "square", "sh" };
    const std::array<const char*, 3> filterNames { "off", "lpf", "hpf" };
    const std::array<const char*, 3> edgeNames   { "both", "pos", "neg" };
    const std::array<const char*, 4> pulseNames  { "wheel", "shaker", "digital", "midiIn" };
    const std::array<const char*, 2> wheelNames  { "speed", "angle" };
    const std::array<const char*, 2> midiInNames { "notes", "clock" };
    const std::array<const char*, 2> startNames  { "clock", "note" };
    const std::array<const char*, 2> shakeNames  { "shake", "follow" };
    const std::array<const char*, 3> genNames    { "arp", "input", "melody" };
    const std::array<const char*, 5> orderNames  { "up", "down", "upDown", "random", "asTyped" };
    const std::array<const char*, 2> runNames    { "free", "resetOnIn" };
    const std::array<const char*, 3> tfNames     { "scale", "set", "held" };
    const std::array<const char*, 8> scaleNames  { "major", "minor", "dorian", "phrygian", "pentMajor", "pentMinor", "wholeTone", "chromatic" };
}

const std::vector<int>& scaleSteps (ScaleType t)
{
    static const std::array<std::vector<int>, 8> tables { {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 2, 4, 6, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
    } };
    return tables[(size_t) t];
}

void Patch::setArpText (const juce::String& text)
{
    arpCount = 0;
    auto p = text.getCharPointer();
    while (! p.isEmpty() && arpCount < maxArpTones)
    {
        const bool neg = *p == '-' && juce::CharacterFunctions::isDigit (p[1]);
        if (neg || p.isDigit())
        {
            if (neg) ++p;
            int v = 0;
            while (p.isDigit() && v < 1000)
                v = v * 10 + (int) (*p++ - '0');
            while (p.isDigit()) ++p;
            arp[(size_t) arpCount++] = (int8_t) juce::jlimit (-24, 24, neg ? -v : v);
        }
        else
        {
            ++p;
        }
    }
}

juce::String Patch::arpText() const
{
    juce::StringArray s;
    for (int i = 0; i < arpCount; ++i)
        s.add (juce::String (arp[(size_t) i]));
    return s.joinIntoString (" ");
}

juce::XmlElement Patch::toXml() const
{
    juce::XmlElement x ("Patch");
    putEnum (x, "voiceMode", voiceMode, voiceNames);
    x.setAttribute ("inChannel", inChannel);
    x.setAttribute ("outChannel", outChannel);
    putEnum (x, "enSource", enSource, sourceNames);
    putEnum (x, "enShape", enShape, shapeNames);
    x.setAttribute ("enSync", enSync);
    x.setAttribute ("enRetrig", enRetrig);
    putEnum (x, "enFilter", enFilter, filterNames);
    putEnum (x, "enEdge", enEdge, edgeNames);
    x.setAttribute ("ccNumber", ccNumber);
    putEnum (x, "pulse", pulse, pulseNames);
    putEnum (x, "wheelMode", wheelMode, wheelNames);
    putEnum (x, "shakeMode", shakeMode, shakeNames);
    x.setAttribute ("digBase", digBase);
    putEnum (x, "midiInSource", midiInSource, midiInNames);
    x.setAttribute ("clockDiv", clockDiv);
    putEnum (x, "clockStart", clockStart, startNames);
    putEnum (x, "gen", gen, genNames);
    putEnum (x, "order", order, orderNames);
    x.setAttribute ("arp", arpText());

    // One token per step: a tone, "_" for a tie, "." for a rest.
    juce::StringArray mel;
    for (auto v : melody)
        mel.add (v == melTie ? "_" : v == melRest ? "." : juce::String (v));
    x.setAttribute ("melody", mel.joinIntoString (" "));
    x.setAttribute ("melTranspose", melTranspose);
    putEnum (x, "runMode", runMode, runNames);
    putEnum (x, "tfMode", tfMode, tfNames);
    x.setAttribute ("root", root);
    putEnum (x, "scale", scale, scaleNames);
    juce::StringArray pc;
    for (int i = 0; i < 12; ++i)
        if (pcs & (1 << i))
            pc.add (juce::String (i));
    x.setAttribute ("pcs", pc.joinIntoString (" "));
    x.setAttribute ("wrap", wrap);
    x.setAttribute ("preview", preview);
    return x;
}

Patch Patch::fromXml (const juce::XmlElement& x)
{
    Patch p;
    getEnum (x, "voiceMode", p.voiceMode, voiceNames);
    p.inChannel = juce::jlimit (0, 16, x.getIntAttribute ("inChannel", p.inChannel));
    p.outChannel = juce::jlimit (1, 16, x.getIntAttribute ("outChannel", p.outChannel));
    getEnum (x, "enSource", p.enSource, sourceNames);
    getEnum (x, "enShape", p.enShape, shapeNames);
    p.enSync = x.getBoolAttribute ("enSync", p.enSync);
    p.enRetrig = x.getBoolAttribute ("enRetrig", p.enRetrig);
    getEnum (x, "enFilter", p.enFilter, filterNames);
    getEnum (x, "enEdge", p.enEdge, edgeNames);
    p.ccNumber = juce::jlimit (0, 119, x.getIntAttribute ("ccNumber", p.ccNumber));
    getEnum (x, "pulse", p.pulse, pulseNames);
    getEnum (x, "wheelMode", p.wheelMode, wheelNames);
    getEnum (x, "shakeMode", p.shakeMode, shakeNames);
    getEnum (x, "midiInSource", p.midiInSource, midiInNames);
    p.clockDiv = juce::jlimit (0, 5, x.getIntAttribute ("clockDiv", p.clockDiv));
    getEnum (x, "clockStart", p.clockStart, startNames);
    const int base = x.getIntAttribute ("digBase", p.digBase);
    if (base == 2 || base == 3 || base == 5 || base == 7)
        p.digBase = base;
    getEnum (x, "gen", p.gen, genNames);
    getEnum (x, "order", p.order, orderNames);
    if (x.hasAttribute ("arp"))
        p.setArpText (x.getStringAttribute ("arp"));

    if (x.hasAttribute ("melody"))
    {
        auto tokens = juce::StringArray::fromTokens (x.getStringAttribute ("melody"), " ", "");
        for (int i = 0; i < melodySteps; ++i)
        {
            const auto t = i < tokens.size() ? tokens[i] : juce::String (".");
            p.melody[(size_t) i] = t == "_" ? melTie : t == "." ? melRest : (int8_t) juce::jlimit (-7, 7, t.getIntValue());
        }
    }
    p.melTranspose = x.getBoolAttribute ("melTranspose", p.melTranspose);
    getEnum (x, "runMode", p.runMode, runNames);
    getEnum (x, "tfMode", p.tfMode, tfNames);
    p.root = juce::jlimit (0, 11, x.getIntAttribute ("root", p.root));
    getEnum (x, "scale", p.scale, scaleNames);
    if (x.hasAttribute ("pcs"))
    {
        p.pcs = 0;
        for (auto& t : juce::StringArray::fromTokens (x.getStringAttribute ("pcs"), " ", ""))
            if (t.isNotEmpty())
                p.pcs |= (uint16_t) (1 << juce::jlimit (0, 11, t.getIntValue()));
    }
    p.wrap = x.getBoolAttribute ("wrap", p.wrap);
    p.preview = x.getBoolAttribute ("preview", p.preview);
    return p;
}
} // namespace mdt
