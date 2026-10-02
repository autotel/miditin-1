#include <juce_audio_basics/juce_audio_basics.h>
#include "Engine/Engine.h"
#include "Engine/PreviewSynth.h"

using namespace mdt;

namespace
{
struct Event { double time; int note; int vel; };   // vel 0 = note-off

/** Drives an engine like a host would: fixed blocks, input scheduled by time. */
struct Rig
{
    Engine e;
    Patch patch;
    ParamValues pv = ParamValues::defaults();
    Transport tr;
    double sr;
    int block;
    double now = 0;
    std::vector<Event> out;
    std::vector<std::pair<double, juce::MidiMessage>> pending;
    juce::MidiBuffer in, o;

    explicit Rig (double sampleRate = 48000, int blockSize = 256) : sr (sampleRate), block (blockSize)
    {
        e.prepare (sr);
    }

    void at (double time, juce::MidiMessage m) { pending.push_back ({ time, m }); }
    void noteOn (double time, int n, int v = 100) { at (time, juce::MidiMessage::noteOn (1, n, (juce::uint8) v)); }
    void noteOff (double time, int n) { at (time, juce::MidiMessage::noteOff (1, n)); }

    void run (double seconds)
    {
        const double end = now + seconds;
        while (now < end - 1e-9)
        {
            in.clear(); o.clear();
            for (auto& [time, m] : pending)
            {
                const int pos = (int) std::floor ((time - now) * sr);
                if (pos >= 0 && pos < block)
                    in.addEvent (m, pos);
            }
            e.process (patch, pv, tr, in, o, block);
            for (const auto md : o)
            {
                const auto m = md.getMessage();
                out.push_back ({ now + md.samplePosition / sr, m.getNoteNumber(), m.isNoteOn() ? m.getVelocity() : 0 });
            }
            now += block / sr;
            if (tr.playing) tr.ppq += block / sr * tr.bpm / 60.0;
        }
    }

    int ons() const { int n = 0; for (auto& ev : out) n += ev.vel > 0; return n; }

    /** Every note-on matched by a later note-off, never two ons without an off between. */
    bool balanced() const
    {
        std::array<int, 128> open {};
        for (auto& ev : out)
        {
            if (ev.vel > 0) { if (open[(size_t) ev.note]++ != 0) return false; }
            else if (open[(size_t) ev.note]-- != 1) return false;
        }
        for (int c : open) if (c != 0) return false;
        return true;
    }
};
} // namespace

struct EngineTests final : juce::UnitTest
{
    EngineTests() : juce::UnitTest ("Engine") {}

    void runTest() override
    {
        beginTest ("default patch: wheel pulses arpeggiate the held chord, upwards");
        {
            Rig r;
            r.noteOn (0, 67); r.noteOn (0, 60); r.noteOn (0, 64);
            r.run (4);
            expect (r.ons() > 5, juce::String (r.ons()));
            std::vector<int> played;
            for (auto& ev : r.out)
                if (ev.vel > 0)
                    played.push_back (ev.note);
            for (size_t i = 0; i < played.size(); ++i)
                expectEquals (played[i], (int[]) { 60, 64, 67 }[i % 3]);
            r.run (2);
            r.patch.run = false;
            r.run (0.1);
            expect (r.balanced());
            expectEquals (r.e.sounding(), 0);
        }

        beginTest ("deterministic: same MIDI at any block size, same notes at any sample rate");
        {
            auto render = [] (double sr, int block) {
                Rig r (sr, block);
                r.patch.pulse = PulseKind::shaker;
                r.patch.voiceMode = VoiceMode::poly;
                r.noteOn (0.5, 60); r.noteOn (0.7, 64); r.noteOff (2.0, 60); r.noteOff (3.0, 64);
                r.run (4);
                return r.out;
            };
            const auto a = render (48000, 64), b = render (48000, 512), c = render (96000, 300);
            expect (a.size() > 4);
            expectEquals ((int) a.size(), (int) b.size());
            expectEquals ((int) a.size(), (int) c.size());
            for (size_t i = 0; i < std::min ({ a.size(), b.size(), c.size() }); ++i)
            {
                expect (a[i].note == b[i].note && a[i].vel == b[i].vel && std::abs (a[i].time - b[i].time) < 1e-9);
                expect (a[i].note == c[i].note && a[i].vel == c[i].vel && std::abs (a[i].time - c[i].time) < 0.0011);
            }
        }

        beginTest ("MIDI-in pulse: each input note is a pulse, velocity carried");
        {
            Rig r;
            r.patch.pulse = PulseKind::midiIn;
            r.noteOn (0.1, 60, 127); r.noteOff (0.2, 60);
            r.noteOn (0.5, 62, 64);  r.noteOff (0.6, 62);
            r.run (1.5);
            expectEquals (r.ons(), 2);
            expectEquals (r.out[0].vel, 127);
            expect (r.balanced());
            expectWithinAbsoluteError (r.out[1].time - r.out[0].time, 0.18, 0.002);   // gate 180 ms
        }

        beginTest ("melody ties: one note-on held across tied steps, released a gate after the last");
        {
            Rig r;
            r.patch.pulse = PulseKind::midiIn;
            r.patch.gen = GenKind::melody;
            r.patch.melody.fill (melRest);
            r.patch.melody[0] = 0; r.patch.melody[1] = melTie; r.patch.melody[2] = melTie;
            for (int i = 0; i < 3; ++i) { r.noteOn (0.1 + i, 40); r.noteOff (0.2 + i, 40); }
            r.run (3.5);
            expectEquals (r.ons(), 1);
            expect (r.balanced());
            expectWithinAbsoluteError (r.out.back().time, 2.1 + 0.18, 0.002);
        }

        beginTest ("silent until input: no note-on while nothing is held, any pulse, mono or poly");
        for (int k = 0; k < 4; ++k)
            for (auto vm : { VoiceMode::mono, VoiceMode::poly })
            {
                Rig r;
                r.patch.pulse = (PulseKind) k;
                r.patch.voiceMode = vm;
                r.pv[P::shAgitate] = 1;
                r.tr = { true, true, 120, 0 };
                r.run (3);
                expectEquals (r.ons(), 0, "pulse kind " + juce::String (k));
                r.noteOn (r.now, 64);
                r.noteOff (r.now + 1.5, 64);
                r.run (2);
                const int during = r.ons();
                expect (k == 3 ? during == 1 : during > 0, "pulse kind " + juce::String (k));
                r.run (3);
                expectEquals (r.ons(), during, "pulse kind " + juce::String (k));
                expect (r.balanced());
            }

        beginTest ("non-note input passes through at its position; input notes do not");
        {
            Rig r;
            r.patch.pulse = PulseKind::midiIn;
            r.patch.enSource = EnergySource::cc;
            const juce::MidiMessage pass[] { juce::MidiMessage::controllerEvent (2, 1, 90),
                                             juce::MidiMessage::pitchWheel (3, 9000),
                                             juce::MidiMessage::channelPressureChange (1, 40),
                                             juce::MidiMessage::programChange (5, 7) };
            juce::MidiBuffer in, o;
            int pos = 10;
            for (auto& m : pass) in.addEvent (m, pos += 20);
            in.addEvent (juce::MidiMessage::noteOn (1, 64, 0.5f), 5);
            r.e.process (r.patch, r.pv, r.tr, in, o, 256);
            int i = 0, notesOut = 0;
            for (const auto md : o)
            {
                const auto m = md.getMessage();
                if (m.isNoteOnOrOff()) { ++notesOut; expect (m.getChannel() == 1); continue; }
                expect (i < 4 && m.getRawDataSize() == pass[i].getRawDataSize()
                        && std::memcmp (m.getRawData(), pass[i].getRawData(), (size_t) m.getRawDataSize()) == 0);
                expectEquals (md.samplePosition, 30 + 20 * i++);
            }
            expectEquals (i, 4);
            expectEquals (notesOut, 1);   // the generated note, not the input one
        }

        beginTest ("a self-running tie is released when the last input note lifts");
        {
            Rig r;
            r.patch.pulse = PulseKind::digital;
            r.pv[P::enDepth] = 0;
            r.patch.gen = GenKind::melody;
            r.patch.melody.fill (melTie);
            r.patch.melody[0] = 0;
            r.noteOn (0.1, 40);
            r.noteOff (1.2, 40);
            r.run (1.15);
            expectEquals (r.e.sounding(), 1);
            r.run (0.1);
            expectEquals (r.e.sounding(), 0);
            expect (r.balanced());
        }

        beginTest ("a tie broken by an edit is released, not stranded");
        {
            Rig r;
            r.patch.pulse = PulseKind::midiIn;
            r.patch.gen = GenKind::melody;
            r.patch.melody.fill (melTie);
            r.patch.melody[0] = 0;
            r.noteOn (0.1, 40);
            r.run (0.5);
            expectEquals (r.e.sounding(), 1);
            r.patch.gen = GenKind::arp;
            r.run (0.1);
            expectEquals (r.e.sounding(), 0);
            expect (r.balanced());
        }

        beginTest ("poly: a chain per held note, tone 0 on the voice's own note, removed on release");
        {
            Rig r;
            r.patch.voiceMode = VoiceMode::poly;
            r.patch.pulse = PulseKind::midiIn;
            r.noteOn (0.1, 64);   // E is in A minor
            r.run (0.3);
            expectEquals (r.e.activeVoices(), 1);
            expectEquals (r.out[0].note, 64);
            r.noteOff (0.4, 64);
            r.run (0.5);
            expectEquals (r.e.activeVoices(), 0);
            expect (r.balanced());
        }

        beginTest ("no hanging notes: note storms, mode switches, voice-cap overflow, transport stop");
        {
            Rig r (44100, 128);
            r.patch.voiceMode = VoiceMode::poly;
            r.patch.gen = GenKind::melody;
            r.patch.melody.fill (melTie);
            r.patch.melody[0] = 0; r.patch.melody[5] = 3;
            r.pv[P::gate] = 1200;
            Rng rng (3);
            for (int i = 0; i < 400; ++i)
            {
                const double tm = i * 0.01;
                const int n = 40 + (int) (rng.next() % 40);
                if (rng.next() % 2) r.noteOn (tm, n, 1 + (int) (rng.next() % 127)); else r.noteOff (tm, n);
            }
            r.tr = { true, true, 120, 0 };
            for (int k = 0; k < 8; ++k)
            {
                r.run (0.5);
                expect (r.e.activeVoices() <= maxVoices);
                r.patch.voiceMode = k % 2 ? VoiceMode::poly : VoiceMode::mono;
                r.patch.pulse = (PulseKind) (k % 4);
                r.patch.gen = (GenKind) (k % 3);
            }
            r.tr.playing = false;
            r.run (0.01);
            expectEquals (r.e.sounding(), 0);
            r.e.releaseHeld (r.o, 0);
            r.run (2);
            expectEquals (r.e.sounding(), 0);
            expect (r.balanced());
        }

        beginTest ("digital locks to the host bar when playing");
        {
            Rig r;
            r.patch.pulse = PulseKind::digital;
            r.pv[P::dgDiv] = 2;               // energy 0.5 x 2 -> row 1: quarter notes
            r.pv[P::enDepth] = 0;
            r.tr = { true, true, 120, 0.0 };  // 0.5 s per beat
            r.noteOn (0, 60);
            r.run (2.05);
            std::vector<double> times;
            for (auto& ev : r.out) if (ev.vel > 0) times.push_back (ev.time);
            expectEquals ((int) times.size(), 4);
            for (size_t i = 0; i < times.size(); ++i)
                expectWithinAbsoluteError (times[i], 0.5 * (double) (i + 1), 0.0015);
        }

        beginTest ("preview synth sounds a note and falls silent after its release");
        {
            PreviewSynth ps;
            ps.prepare (48000);
            juce::AudioBuffer<float> a (2, 4800);
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
            m.addEvent (juce::MidiMessage::noteOff (1, 69), 2400);
            a.clear();
            ps.render (m, a);
            expect (a.getMagnitude (0, 2400) > 0.05f);
            juce::AudioBuffer<float> tail (2, 48000);
            tail.clear();
            ps.render ({}, tail);
            expect (tail.getMagnitude (24000, 24000) < 1e-4f);
        }

        beginTest ("snapshot carries views, held notes and the last mapping");
        {
            Rig r;
            r.patch.voiceMode = VoiceMode::poly;
            r.noteOn (0.0, 60); r.noteOn (0.0, 67);
            r.run (0.5);
            UiSnapshot s;
            r.e.fillSnapshot (s);
            expectEquals (s.numViews, 2);
            expectEquals (s.heldCount, 2);
            expect (s.poly);
        }

        beginTest ("audio source: input RMS is the energy, mono or stereo");
        for (int channels : { 1, 2 })
        {
            Engine e;
            e.prepare (48000);
            Patch p; p.enSource = EnergySource::audio;
            ParamValues pv = ParamValues::defaults();
            pv[P::enCenter] = 0.5f; pv[P::enDepth] = 1;
            juce::AudioBuffer<float> audio (channels, 480);
            juce::MidiBuffer in, o;
            UiSnapshot s;
            double phase = 0;
            auto run = [&] (double seconds, float amp) {
                for (int b = 0; b < (int) std::round (seconds * 100); ++b)
                {
                    for (int i = 0; i < 480; ++i, phase += 440.0 / 48000)
                        for (int ch = 0; ch < channels; ++ch)
                            audio.setSample (ch, i, amp * (float) std::sin (6.283185307179586 * phase));
                    o.clear();
                    e.process (p, pv, {}, in, o, 480, audio.getArrayOfReadPointers(), channels);
                }
                e.fillSnapshot (s);
                return s.views[0].energy;
            };
            expectWithinAbsoluteError (run (0.2, 0.5f), 0.5f / std::sqrt (2.0f), 0.02f);
            expectWithinAbsoluteError (run (0.3, 0), 0.0f, 0.01f);
            run (0.01, std::numeric_limits<float>::quiet_NaN());
            expect (std::isfinite (run (0.01, 0)), "a NaN in the input does not stick");
        }
    }
};

static EngineTests engineTests;
