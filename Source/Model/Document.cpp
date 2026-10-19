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
        // Tags are comma-joined, as the librarian stores them. "harmonic passthrough": the output
        // stays on the held notes; "fixed scale": it is forced into a scale whatever is played.
        auto add = [&v] (juce::String name, juce::String author, juce::String description, juce::String tags,
                         auto&& tweak) {
            FactoryPreset f { std::move (name), "Factory", std::move (author), std::move (description),
                              juce::StringArray::fromTokens (tags, ",", ""), {}, ParamValues::defaults() };
            tweak (f.patch, f.pv);
            v.push_back (f);
        };

        // The defaults: the wheel paces an arpeggio of the held notes.
        add ("Wheel arpeggio", "Claude", "The defaults: the wheel paces an arpeggio of your held notes.",
             "harmonic passthrough,organic", [] (Patch&, ParamValues&) {});

        add ("Shaker melody", "Claude", "A slow triangle LFO shakes four balls through the melody, in A minor.",
             "melodic,organic,fixed scale", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::shaker;
            p.gen = GenKind::melody;
            p.tfMode = TransformMode::scale;
            p.enShape = Shape::tri;
            pv[P::enRate] = 0.08f;
            pv[P::shBalls] = 4;
            pv[P::gate] = 240;
        });

        add ("Digital triplets", "Claude", "A synced LFO moves a triplet grid; an up/down arpeggio in D dorian.",
             "straight,fixed scale", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::digital;
            p.digBase = 3;
            p.gen = GenKind::arp;
            p.tfMode = TransformMode::scale;
            p.enSync = true;
            p.order = Order::upDown;
            p.scale = ScaleType::dorian;
            p.root = 2;
            p.setArpText ("0 2 4 6 7");
            pv[P::enDiv] = 1;
            pv[P::dgDiv] = 3;
            pv[P::gate] = 120;
        });

        add ("MIDI in, held chord", "Claude", "Poly: each key you play is one step of 0 1 2 4, in A minor pentatonic.",
             "poly,fixed scale", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::midiIn;
            p.voiceMode = VoiceMode::poly;
            p.gen = GenKind::arp;
            p.tfMode = TransformMode::scale;
            p.setArpText ("0 1 2 4");
            p.order = Order::asTyped;
            p.scale = ScaleType::pentMinor;
            pv[P::gate] = 400;
        });

        add ("Gestural arpeggio", "autotel", "How hard you play spins the wheel through the held notes, with inertia: "
                                  "dig in to rush it, ease off to let it slow.",
             "velocity sensitive,harmonic passthrough,organic", [] (Patch& p, ParamValues& pv) {
            p.enSource = EnergySource::vel;
            p.enRetrig = true;
            p.enFilter = EnergyFilter::lpf;
            pv[P::enDepth] = 1;
            pv[P::enFilterAmt] = 0.66f;
            pv[P::whSpeed] = 0.99f;
            pv[P::whModOffset] = 0.17f;
            pv[P::velEnergy] = 0.21f;
            pv[P::velPulse] = 0.52f;
            pv[P::velInput] = 0.51f;
        });

        add ("Water-like", "autotel", "A slow swell shakes the balls; the held notes spill out in drips and splashes.",
             "harmonic passthrough,velocity sensitive,harsh", [] (Patch& p, ParamValues& pv) {
            p.pulse = PulseKind::shaker;
            p.enRetrig = true;
            pv[P::enRate] = 0.17f;
        });

        add ("Strummer", "autotel", "Simulate strumming. Adjust speed by changing energy's filter.",
             "harmonic passthrough,organic,velocity sensitive", [] (Patch& p, ParamValues& pv) {
            p.enSource = EnergySource::vel;
            p.enRetrig = true;
            p.enFilter = EnergyFilter::lpf;
            p.wheelMode = WheelMode::angle;
            pv[P::enDepth] = 1;
            pv[P::enFilterAmt] = 0.27f;
            pv[P::enPhase] = 0.3f;
            pv[P::whSpeed] = 1.6f;
            pv[P::whSlices] = 18;
            pv[P::velEnergy] = 0.2f;
            pv[P::velPulse] = 0.24f;
            pv[P::velInput] = 0.75f;
        });

        add ("PolyChaos", "autotel", "Generates beautiful decorations to chords and melodies.",
             "melodic,harmonic passthrough,organic,poly,velocity sensitive", [] (Patch& p, ParamValues& pv) {
            p.voiceMode = VoiceMode::poly;
            p.enSource = EnergySource::vel;
            p.enFilter = EnergyFilter::lpf;
            p.pulse = PulseKind::shaker;
            p.gen = GenKind::melody;
            p.melody[6] = 2;
            p.melody[7] = melTie;
            p.melTranspose = true;
            pv[P::enDepth] = 1;
            pv[P::whSpeed] = 1.01f;
            pv[P::gate] = 820;
            pv[P::velEnergy] = 0.3f;
            pv[P::velPulse] = 0.16f;
            pv[P::velInput] = 0.71f;
        });

        add ("Maximal Minimal", "autotel", "Arpeggiate with chords: each held note runs 0 1 2 4 in A minor pentatonic, "
                                "on the DAW's clock -- press play.",
             "poly,straight,fixed scale", [] (Patch& p, ParamValues& pv) {
            p.voiceMode = VoiceMode::poly;
            p.pulse = PulseKind::midiIn;
            p.midiInSource = MidiInSource::clock;
            p.gen = GenKind::arp;
            p.order = Order::asTyped;
            p.setArpText ("0 1 2 4");
            p.tfMode = TransformMode::scale;
            p.scale = ScaleType::pentMinor;
            pv[P::gate] = 400;
            pv[P::velEnergy] = 0.23f;
            pv[P::velPulse] = 0.17f;
            pv[P::velInput] = 0.28f;
        });

        add ("Mechanistic Arpeggio", "autotel", "Works like a conventional arpeggiator; the mod wheel picks the subdivision.",
             "harmonic passthrough,straight", [] (Patch& p, ParamValues& pv) {
            p.enSource = EnergySource::cc;
            p.pulse = PulseKind::digital;
            p.order = Order::asTyped;
            pv[P::velEnergy] = 0.35f;
            pv[P::velPulse] = 0.31f;
            pv[P::velInput] = 0.3f;
        });
        return v;
    }();
    return presets;
}

juce::MemoryBlock factoryPresetData (const FactoryPreset& f, const atl::PresetFormat& format)
{
    auto doc = makeDocument (f.patch, f.pv);
    doc.setProperty (format.nameProperty, f.name, nullptr);
    doc.setProperty (format.authorProperty, f.author, nullptr);
    atl::PresetMetadata meta;
    meta.description = f.description;
    meta.tags = f.tags;
    doc.appendChild (meta.toValueTree(), nullptr);
    juce::MemoryOutputStream out;
    doc.writeToStream (out);
    return out.getMemoryBlock();
}
} // namespace mdt
