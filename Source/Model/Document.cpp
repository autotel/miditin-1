#include "Document.h"

namespace mdt
{
juce::ValueTree makeDocument (const Patch& patch, const ParamValues& pv)
{
    juce::ValueTree doc (documentType), values ("Params");
    for (int i = 0; i < numParams; ++i)
        values.setProperty (params()[(size_t) i].id, pv.v[(size_t) i], nullptr);
    doc.appendChild (values, nullptr);
    doc.appendChild (juce::ValueTree::fromXml (patch.toXml()), nullptr);
    return doc;
}

void readDocument (const juce::ValueTree& doc, Patch& patch, ParamValues& pv)
{
    patch = {};
    pv = ParamValues::defaults();
    if (! doc.hasType (documentType))
        return;

    const auto values = doc.getChildWithName ("Params");
    for (int i = 0; i < numParams; ++i)
    {
        const auto& s = params()[(size_t) i];
        if (const auto* v = values.getPropertyPointer (s.id))
            pv.v[(size_t) i] = juce::jlimit (s.min, s.max, (float) *v);
    }
    if (const auto p = doc.getChildWithName ("Patch"); p.isValid())
        if (const auto xml = p.createXml())
            patch = Patch::fromXml (*xml);
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> v;
        auto add = [&v] (juce::String name, auto&& tweak) {
            FactoryPreset f { std::move (name), "Factory", {}, ParamValues::defaults() };
            tweak (f.patch, f.pv);
            v.push_back (f);
        };

        // The prototype's opening state.
        add ("Wheel arpeggio", [] (Patch&, ParamValues&) {});

        add ("Shaker melody", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::shaker;
            p.gen = GenKind::melody;
            p.enShape = Shape::tri;
            pv[P::enRate] = 0.08f;
            pv[P::shBalls] = 4;
            pv[P::gate] = 240;
        });

        add ("Digital triplets", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::digital;
            p.digBase = 3;
            p.enSync = true;
            p.order = Order::upDown;
            p.scale = ScaleType::dorian;
            p.root = 2;
            p.setArpText ("0 2 4 6 7");
            pv[P::enDiv] = 1;
            pv[P::dgDiv] = 3;
            pv[P::gate] = 120;
        });

        add ("MIDI in, held chord", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::midiIn;
            p.voiceMode = VoiceMode::poly;
            p.setArpText ("0 1 2 4");
            p.order = Order::asTyped;
            p.scale = ScaleType::pentMinor;
            pv[P::gate] = 400;
        });
        return v;
    }();
    return presets;
}
} // namespace mdt
