#include <set>
#include <thread>
#include "Model/BuildInfo.h"
#include "Model/ParamRegistry.h"
#include "Model/Patch.h"
#include "Model/TripleBuffer.h"

using namespace mdt;

struct ParamRegistryTests final : juce::UnitTest
{
    ParamRegistryTests() : juce::UnitTest ("ParamRegistry") {}

    void runTest() override
    {
        beginTest ("ids unique, defaults inside their ranges, normalise round-trips");
        std::set<std::string> ids;
        for (auto& s : params())
        {
            expect (ids.insert (s.id).second, s.id);
            expect (s.min < s.max && s.def >= s.min && s.def <= s.max, s.id);
            expectWithinAbsoluteError (toNatural (s, toNormalised (s, s.def)), s.def, 1e-4f);
        }

        beginTest ("readouts match the prototype");
        expectEquals (juce::String (format (P::enDiv, 2)), juce::String ("1 bar"));
        expectEquals (juce::String (format (P::dgDiv, 4, 3)), juce::String ("1/48"));
        expectEquals (juce::String (format (P::enFilterAmt, 0.3f)), juce::String ("0.29s"));
        expectEquals (juce::String (format (P::gate, 180)), juce::String ("180ms"));
    }
};

struct PatchTests final : juce::UnitTest
{
    PatchTests() : juce::UnitTest ("Patch") {}

    void runTest() override
    {
        beginTest ("arp text: every integer, clamped, at most 16");
        Patch p;
        p.setArpText ("0, -3 x7 99 --2");
        expectEquals (p.arpCount, 5);
        expectEquals ((int) p.arp[1], -3);
        expectEquals ((int) p.arp[2], 7);
        expectEquals ((int) p.arp[3], 24);
        expectEquals ((int) p.arp[4], -2);
        p.setArpText ("1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18");
        expectEquals (p.arpCount, 16);
        p.setArpText ("");
        expectEquals (p.arpCount, 0);

        beginTest ("XML round-trip keeps every field");
        Patch q;
        q.voiceMode = VoiceMode::poly; q.inChannel = 9; q.outChannel = 12; q.enSource = EnergySource::cc; q.enShape = Shape::sh; q.shakeMode = ShakeMode::follow;
        q.enSync = true; q.enRetrig = true; q.enFilter = EnergyFilter::hpf; q.enEdge = EnergyEdge::neg; q.ccNumber = 74;
        q.pulse = PulseKind::digital; q.wheelMode = WheelMode::angle; q.midiInSource = MidiInSource::clock; q.clockDiv = 5; q.clockStart = ClockStart::note; q.legato = false;
        q.digBase = 5; q.gen = GenKind::melody; q.order = Order::asTyped; q.setArpText ("-7 0 12");
        q.melody.fill (melRest); q.melody[0] = -7; q.melody[1] = melTie; q.melody[15] = 7;
        q.melTranspose = true; q.runMode = RunMode::resetOnIn; q.tfMode = TransformMode::set; q.root = 2;
        q.scale = ScaleType::pentMinor; q.pcs = 0x0a5; q.wrap = false; q.preview = true;
        expect (Patch::fromXml (q.toXml()) == q);

        beginTest ("unreadable fields keep defaults");
        juce::XmlElement junk ("Patch");
        junk.setAttribute ("pulse", "nonsense");
        junk.setAttribute ("digBase", 4);
        expect (Patch::fromXml (junk) == Patch {});
    }
};

struct TripleBufferTests final : juce::UnitTest
{
    TripleBufferTests() : juce::UnitTest ("TripleBuffer") {}

    void runTest() override
    {
        beginTest ("reader sees the latest publish, and nothing new without one");
        TripleBuffer<int> b;
        expect (! b.fetch());
        b.back() = 1; b.publish();
        b.back() = 2; b.publish();
        expect (b.fetch());
        expectEquals (b.front(), 2);
        expect (! b.fetch());

        beginTest ("concurrent: the reader never sees a torn or stale-after-fresh value");
        struct Pair { int a = 0, b = 0; };
        TripleBuffer<Pair> t;
        std::atomic<bool> done { false };
        std::thread writer ([&] {
            for (int i = 1; i <= 200000; ++i) { t.back() = { i, -i }; t.publish(); }
            done = true;
        });
        int last = 0, torn = 0, backwards = 0;
        while (! done || t.fetch())
            if (t.fetch())
            {
                const auto p = t.front();
                torn += p.a != -p.b;
                backwards += p.a < last;
                last = p.a;
            }
        writer.join();
        expectEquals (torn, 0);
        expectEquals (backwards, 0);
    }
};

struct BuildInfoTests final : juce::UnitTest
{
    BuildInfoTests() : juce::UnitTest ("BuildInfo") {}

    void runTest() override
    {
        beginTest ("a SHA and a date-time are stamped in");
        const juce::String sha (buildSha()), date (buildDate());
        expect (sha.isNotEmpty(), sha);
        expect (date.length() == 16 && date[4] == '-' && date[7] == '-' && date[10] == ' ' && date[13] == ':'
                    && date.substring (0, 4).getIntValue() >= 2026, date);
    }
};

static ParamRegistryTests paramRegistryTests;
static BuildInfoTests buildInfoTests;
static PatchTests patchTests;
static TripleBufferTests tripleBufferTests;

#include "Model/Document.h"

struct DocumentTests final : juce::UnitTest
{
    DocumentTests() : juce::UnitTest ("Document") {}

    void runTest() override
    {
        beginTest ("round-trip: every parameter and the patch");
        Patch p;
        p.pulse = PulseKind::digital;
        p.melody[3] = melTie;
        auto pv = ParamValues::defaults();
        for (int i = 0; i < numParams; ++i)
            pv.v[(size_t) i] = params()[(size_t) i].max;
        Patch p2;
        ParamValues pv2;
        readDocument (makeDocument (p, pv), p2, pv2);
        expect (p2 == p);
        expect (pv2.v == pv.v);

        beginTest ("an invalid tree reads as the defaults; out-of-range values are clamped");
        readDocument ({}, p2, pv2);
        expect (p2 == Patch {});
        expect (pv2.v == ParamValues::defaults().v);
        auto doc = makeDocument ({}, ParamValues::defaults());
        doc.getChildWithName ("Params").setProperty ("wh_slices", 99, nullptr);
        readDocument (doc, p2, pv2);
        expectEquals (pv2.i (P::whSlices), 24);

        beginTest ("factory presets: one per pulse generator, each a valid document");
        std::set<PulseKind> kinds;
        for (const auto& f : factoryPresets())
        {
            kinds.insert (f.patch.pulse);
            readDocument (makeDocument (f.patch, f.pv), p2, pv2);
            expect (p2 == f.patch, f.name);
        }
        expectEquals ((int) kinds.size(), 4);
    }
};

static DocumentTests documentTests;
