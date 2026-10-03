#include <juce_audio_basics/juce_audio_basics.h>
#include "Engine/Engine.h"
#include "Engine/PreviewSynth.h"

using namespace mdt;

namespace
{
struct Event { double time; int note; int vel; int ch = 1; };   // vel 0 = note-off

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
    void noteOn (double time, int n, int v = 100, int ch = 1) { at (time, juce::MidiMessage::noteOn (ch, n, (juce::uint8) v)); }
    void noteOff (double time, int n, int ch = 1) { at (time, juce::MidiMessage::noteOff (ch, n)); }

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
                out.push_back ({ now + md.samplePosition / sr, m.getNoteNumber(), m.isNoteOn() ? m.getVelocity() : 0, m.getChannel() });
            }
            now += block / sr;
            if (tr.playing) tr.ppq += block / sr * tr.bpm / 60.0;
        }
    }

    int ons() const { int n = 0; for (auto& ev : out) n += ev.vel > 0; return n; }

    /** Every note-on matched by a later note-off on its channel, never two ons without an off between. */
    bool balanced() const
    {
        std::array<int, 17 * 128> open {};
        for (auto& ev : out)
        {
            auto& o = open[(size_t) (ev.ch * 128 + ev.note)];
            if (ev.vel > 0) { if (o++ != 0) return false; }
            else if (o-- != 1) return false;
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
            const int chord[] { 60, 64, 67 };
            for (size_t i = 0; i < played.size(); ++i)
                expectEquals (played[i], chord[i % 3]);
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

        beginTest ("MIDI-in CLOCK: a pulse per division of 0xF8 ticks, or of the host's PPQ without them");
        {
            Rig r;
            r.patch.pulse = PulseKind::midiIn;
            r.patch.midiInSource = MidiInSource::clock;
            r.patch.clockDiv = 1;   // 1/8: every 12 ticks
            r.noteOn (0.0, 60);     // a note is no pulse under CLOCK
            r.at (0.05, juce::MidiMessage::midiStart());
            for (int i = 0; i < 48; ++i)   // 120 bpm: 24 ticks per 0.5 s
                r.at (0.1 + i * 0.5 / 24, juce::MidiMessage::midiClock());
            r.run (1.2);
            expectEquals (r.ons(), 4);
            for (auto& ev : r.out) if (ev.vel > 0) expect (ev.time > 0.099);

            Rig h;
            h.patch.pulse = PulseKind::midiIn;
            h.patch.midiInSource = MidiInSource::clock;
            h.patch.clockDiv = 3;   // 1/16
            h.tr = { true, true, 120, 0 };
            h.noteOn (0.0, 60);
            h.run (2.0 - 1e-3);     // four beats, the grid point at beat 4 not yet reached
            expectEquals (h.ons(), 16);
            std::vector<int> vels;
            for (auto& ev : h.out) if (ev.vel > 0) vels.push_back (ev.vel);
            expect (vels.size() > 4 && vels[0] == 127 && vels[1] < vels[0] && vels[4] == 127);
        }

        beginTest ("CLOCK START = NOTE: the count starts at the note, with or without 0xF8, stopped or playing");
        {
            auto onTimes = [] (const Rig& r) { std::vector<double> v; for (auto& ev : r.out) if (ev.vel > 0) v.push_back (ev.time); return v; };
            for (bool playing : { false, true })
            {
                Rig r;
                r.patch.pulse = PulseKind::midiIn;
                r.patch.midiInSource = MidiInSource::clock;
                r.patch.clockStart = ClockStart::note;
                r.patch.clockDiv = 3;   // 1/16: 0.125 s at 120 bpm
                r.pv[P::bpm] = 120;
                r.tr = { playing, playing, 120, 0 };
                r.noteOn (0.31, 60);    // off the host's grid
                r.noteOff (1.30, 60);
                r.run (1.8);
                const auto on = onTimes (r);
                expectEquals ((int) on.size(), 8);
                for (size_t i = 0; i < on.size(); ++i)
                    expectWithinAbsoluteError (on[i], 0.31 + (double) i * 0.125, 0.003);
                std::vector<int> vels;
                for (auto& ev : r.out) if (ev.vel > 0) vels.push_back (ev.vel);
                expect (vels[0] == 127 && vels[1] < 127 && vels[4] == 127);   // the note is the downbeat
                expect (r.balanced());
            }

            Rig c;
            c.patch.pulse = PulseKind::midiIn;
            c.patch.midiInSource = MidiInSource::clock;
            c.patch.clockStart = ClockStart::note;
            c.patch.clockDiv = 1;   // 1/8: every 12 ticks
            for (int i = 0; i < 48; ++i)   // 120 bpm: 24 ticks per 0.5 s
                c.at (0.1 + i * 0.5 / 24, juce::MidiMessage::midiClock());
            c.noteOn (0.33, 60);    // between ticks 11 and 12
            c.run (1.1);
            const auto on = onTimes (c);
            expectEquals ((int) on.size(), 4);   // the note, then ticks 12, 24, 36 after it
            expectWithinAbsoluteError (on[0], 0.33, 0.002);
            expectWithinAbsoluteError (on[1], 0.1 + 23 * 0.5 / 24, 0.002);   // no second pulse at the first tick

            Rig p;
            p.patch.voiceMode = VoiceMode::poly;
            p.patch.pulse = PulseKind::midiIn;
            p.patch.midiInSource = MidiInSource::clock;
            p.patch.clockStart = ClockStart::note;
            p.patch.clockDiv = 3;
            p.patch.legato = false;   // each voice counts from its own note
            p.pv[P::bpm] = 120;
            p.noteOn (0.0, 60);
            p.noteOn (0.06, 67);
            p.run (0.47);           // 0..0.375 and 0.06..0.435
            int onGrid = 0;
            for (double t : onTimes (p))
            {
                const double a = std::fmod (t + 0.01, 0.125) - 0.01, b = std::fmod (t - 0.06 + 0.01, 0.125) - 0.01;
                onGrid += std::abs (a) < 0.003 || std::abs (b) < 0.003;
            }
            expectEquals ((int) onTimes (p).size(), 8);   // each voice on its own grid
            expectEquals (onGrid, 8);
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

        beginTest ("velocity: pulse strength by default; input, energy and the clamp by their amounts");
        {
            auto firstVel = [] (auto&& setup) {
                Rig r;
                r.patch.pulse = PulseKind::midiIn;
                r.patch.enSource = EnergySource::note;   // C2..C7 -> 0..1, so note 60 is 0.5
                r.pv[P::enCenter] = 0.5f; r.pv[P::enDepth] = 1;
                setup (r);
                r.run (0.5);
                for (auto& ev : r.out) if (ev.vel > 0) return ev.vel;
                return -1;
            };
            const auto press = [] (Rig& r, int v) { r.noteOn (0.1, 60, v); };
            expectEquals (firstVel ([&] (Rig& r) { press (r, 80); }), 80);   // strength = input under MIDI-in
            expectEquals (firstVel ([&] (Rig& r) { press (r, 80); r.pv[P::velPulse] = 0; r.pv[P::velEnergy] = 1; }), 64);
            expectEquals (firstVel ([&] (Rig& r) { press (r, 127); r.pv[P::velEnergy] = 1; }), 127);
            expectEquals (firstVel ([&] (Rig& r) { press (r, 1); r.pv[P::velPulse] = 0; }), 1);

            // Mono, wheel-paced: INPUT follows the latest still-held note, through a later release.
            Rig r;
            r.pv[P::velPulse] = 0; r.pv[P::velInput] = 1;
            r.noteOn (0.0, 60, 40); r.noteOn (0.0, 64, 100); r.noteOn (0.0, 67, 127);
            r.noteOff (2, 67);
            r.run (4);
            std::vector<int> before, after;
            for (auto& ev : r.out) if (ev.vel > 0) (ev.time < 2 ? before : after).push_back (ev.vel);
            expect (! before.empty() && ! after.empty());
            for (int v : before) expectEquals (v, 127);
            for (int v : after) expectEquals (v, 100);
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

        beginTest ("blend: continuous state between two snapshots, discrete state from the newer");
        {
            UiSnapshot a, b, out;
            a.t = 1; b.t = 2; a.serial = 1; b.serial = 2;
            a.views[0].wheelA = 0; b.views[0].wheelA = 4;
            a.views[0].ballCount = b.views[0].ballCount = 1;
            a.views[0].balls[0].x = 0.2f; b.views[0].balls[0].x = 0.6f;
            a.views[0].beat = 3.5; b.views[0].beat = 0;   // a relock is not swept through
            b.views[0].arpI = 3;
            blend (a, b, 1.25, out);
            expectWithinAbsoluteError (out.t, 1.25, 1e-9);
            expectWithinAbsoluteError (out.views[0].wheelA, 1.0, 1e-9);
            expectWithinAbsoluteError (out.views[0].balls[0].x, 0.3f, 1e-6f);
            expectEquals (out.views[0].beat, 0.0);
            expectEquals (out.views[0].arpI, 3);
            blend (a, b, 9, out);
            expectEquals (out.t, 2.0);
            // A bounce off the left wall in between: drawn reaching the wall, never through it.
            a.views[0].balls[0] = { 0.1f, 0.5f, -2, 0 };
            b.views[0].balls[0] = { 0.08f, 0.5f, 2, 0 };
            float lowest = 1;
            for (double t = 1; t <= 2; t += 0.01)
            {
                blend (a, b, t, out);
                lowest = std::min (lowest, out.views[0].balls[0].x);
            }
            expectWithinAbsoluteError (lowest, Shaker::radius, 0.003f);
            b.numViews = 2;   // a voice came or went: no blending across it
            blend (a, b, 1.5, out);
            expectEquals (out.t, 2.0);
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

        beginTest ("RETRIG restarts the LFO at PHASE, free or synced, mono or poly");
        for (bool sync : { false, true })
            for (auto mode : { VoiceMode::mono, VoiceMode::poly })
                for (float phase : { 0.0f, 0.5f })
                {
                    Rig r;
                    r.patch.voiceMode = mode;
                    r.patch.enShape = Shape::saw;
                    r.patch.enSync = sync;
                    r.patch.enRetrig = true;
                    r.pv[P::enRate] = 0.02f;
                    r.pv[P::enCenter] = 0.5f; r.pv[P::enDepth] = 1;
                    r.pv[P::enPhase] = phase;
                    r.noteOn (0.3, 60);
                    r.run (0.32);
                    UiSnapshot s;
                    r.e.fillSnapshot (s);
                    expectWithinAbsoluteError (s.views[0].energyRaw, phase, 0.02f);
                }

        beginTest ("LEGATO: a note under a held note continues the LFO; off, or after a gap, it restarts");
        {
            // Saw at 0.2 Hz from PHASE 0: a second note at 1.3 reads ~0.2 if it continued, ~0 if it restarted.
            auto lowest = [] (bool legato, VoiceMode mode, bool gap) {
                Rig r;
                r.patch.voiceMode = mode;
                r.patch.legato = legato;
                r.patch.enShape = Shape::saw;
                r.patch.enRetrig = true;
                r.pv[P::enRate] = 0.2f;
                r.pv[P::enCenter] = 0.5f; r.pv[P::enDepth] = 1;
                r.pv[P::enPhase] = 0;
                r.noteOn (0.3, 60);
                if (gap) r.noteOff (0.8, 60);
                r.noteOn (1.3, 64);
                r.run (1.32);
                UiSnapshot s;
                r.e.fillSnapshot (s);
                float lo = 1;
                for (int i = 0; i < s.numViews; ++i) lo = std::min (lo, s.views[(size_t) i].energyRaw);
                return lo;
            };
            for (auto mode : { VoiceMode::mono, VoiceMode::poly })
            {
                expectGreaterThan (lowest (true, mode, false), 0.15f);
                expectLessThan (lowest (false, mode, false), 0.05f);
                expectLessThan (lowest (true, mode, true), 0.05f);
            }
        }

        beginTest ("LEGATO: a note under a held note joins the note clock; off, it starts the count over");
        for (bool legato : { true, false })
            for (auto mode : { VoiceMode::mono, VoiceMode::poly })
            {
                Rig r;
                r.patch.voiceMode = mode;
                r.patch.legato = legato;
                r.patch.pulse = PulseKind::midiIn;
                r.patch.midiInSource = MidiInSource::clock;
                r.patch.clockStart = ClockStart::note;
                r.patch.clockDiv = 3;   // 1/16: 0.125 s at 120 bpm
                r.pv[P::bpm] = 120;
                r.noteOn (0.0, 60);
                r.noteOn (0.06, 64);    // off the first note's grid
                r.run (0.47);
                bool offGrid = false;
                for (auto& ev : r.out)
                    if (ev.vel > 0)
                    {
                        const double a = std::fmod (ev.time + 0.01, 0.125) - 0.01;
                        offGrid = offGrid || std::abs (a) > 0.003;
                    }
                expect (offGrid != legato);
            }

        beginTest ("CC LEARN: the last distinct CC numbers, newest first, ours only, never 120+");
        {
            Rig r;
            r.patch.inChannel = 1;
            for (int n : { 1, 74, 1, 11 })
                r.at (0.01, juce::MidiMessage::controllerEvent (1, n, 64));
            r.at (0.01, juce::MidiMessage::controllerEvent (2, 20, 64));    // another channel
            r.at (0.01, juce::MidiMessage::controllerEvent (1, 121, 0));    // reset all controllers
            r.run (0.05);
            UiSnapshot s;
            r.e.fillSnapshot (s);
            expectEquals (s.recentCcCount, 3);
            expect (s.recentCc[0] == 11 && s.recentCc[1] == 1 && s.recentCc[2] == 74);
            for (int n : { 2, 3 })
                r.at (0.06, juce::MidiMessage::controllerEvent (1, n, 64));
            r.run (0.05);
            r.e.fillSnapshot (s);
            expectEquals (s.recentCcCount, 4);   // full: the oldest, 74, dropped
            expect (s.recentCc[0] == 3 && s.recentCc[1] == 2 && s.recentCc[2] == 11 && s.recentCc[3] == 1);
        }

        beginTest ("CH: OMNI poly keeps each voice's channel; a set channel plays only its own");
        {
            Rig r;
            r.patch.voiceMode = VoiceMode::poly;
            r.noteOn (0.0, 60, 100, 3); r.noteOn (0.0, 67, 100, 5);
            r.noteOff (2.0, 60, 3); r.noteOff (2.0, 67, 5);
            r.run (3.0);
            expect (r.ons() > 0 && r.balanced());
            std::array<int, 128> ch {};
            bool consistent = true;
            for (auto& ev : r.out)
            {
                if (ev.vel > 0) ch[(size_t) ev.note] = ev.ch;
                else consistent = consistent && ev.ch == ch[(size_t) ev.note];   // off on its on's channel
            }
            expect (consistent);
            expect (std::all_of (r.out.begin(), r.out.end(), [] (auto& ev) { return ev.ch == 3 || ev.ch == 5; }));
            expect (std::any_of (r.out.begin(), r.out.end(), [] (auto& ev) { return ev.ch == 3; }));
            expect (std::any_of (r.out.begin(), r.out.end(), [] (auto& ev) { return ev.ch == 5; }));
        }
        for (auto mode : { VoiceMode::mono, VoiceMode::poly })
        {
            Rig r;
            r.patch.voiceMode = mode;
            r.patch.inChannel = 2;
            r.noteOn (0.0, 40, 90, 1);    // another channel: straight through
            r.noteOn (0.0, 60, 100, 2);
            r.noteOn (0.0, 64, 100, 2);
            r.noteOff (1.0, 40, 1);
            r.run (1.5);
            int passed = 0;
            for (auto& ev : r.out)
                if (ev.note == 40) { expectEquals (ev.ch, 1); ++passed; }
                else expectEquals (ev.ch, 2);
            expectEquals (passed, 2);
            expect (r.ons() > 1);
        }

        beginTest ("CH: poly OMNI keeps the same note on two channels apart; mono OMNI plays on OUT");
        {
            Rig r;
            r.patch.voiceMode = VoiceMode::poly;
            r.noteOn (0.0, 60, 100, 3); r.noteOn (0.0, 60, 100, 5);
            r.noteOff (1.0, 60, 3);
            r.run (0.5);
            expectEquals (r.e.activeVoices(), 2);
            expectEquals (r.e.heldCount(), 2);
            r.run (0.8);
            expectEquals (r.e.activeVoices(), 1);
            const auto after = r.out.size();
            r.run (1.0);
            expect (r.ons() > 0);
            for (size_t i = after; i < r.out.size(); ++i)
                if (r.out[i].vel > 0) expectEquals (r.out[i].ch, 5);
            r.noteOff (r.now, 60, 5);
            r.run (1.0);
            expect (r.balanced());
            expectEquals (r.e.sounding(), 0);
        }
        {
            Rig r;
            r.patch.outChannel = 7;
            r.noteOn (0.0, 60, 100, 3); r.noteOn (0.0, 64, 100, 5);
            r.run (3.0);
            expect (r.ons() > 0);
            expect (std::all_of (r.out.begin(), r.out.end(), [] (auto& ev) { return ev.ch == 7; }));
        }

        beginTest ("CH: changing channel with notes held releases everything");
        {
            Rig r;
            r.patch.voiceMode = VoiceMode::poly;
            r.patch.pulse = PulseKind::digital;
            r.noteOn (0.0, 60);
            r.run (1.0);
            r.patch.inChannel = 4;
            r.run (0.01);
            expectEquals (r.e.sounding(), 0);
            expectEquals (r.e.heldCount(), 0);
            r.noteOff (1.02, 60);   // now another channel's: passes through
            r.run (1.0);
            expectEquals (r.e.sounding(), 0);
        }

        beginTest ("VEL under LPF: the energy rises from zero to the note's velocity");
        for (auto mode : { VoiceMode::mono, VoiceMode::poly })
        {
            Rig r;
            r.patch.voiceMode = mode;
            r.patch.enSource = EnergySource::vel;
            r.patch.enFilter = EnergyFilter::lpf;
            r.pv[P::enCenter] = 0.5f; r.pv[P::enDepth] = 1;
            r.pv[P::enFilterAmt] = 0.5f;   // tau 0.77 s
            r.noteOn (0.5, 60, 127);
            r.run (0.53);
            UiSnapshot s;
            r.e.fillSnapshot (s);
            expect (s.views[0].energy < 0.1f, "starts from zero");
            r.run (5.0);
            r.e.fillSnapshot (s);
            expectWithinAbsoluteError (s.views[0].energy, 1.0f, 0.02f);
        }

        beginTest ("audio source: input RMS times GAIN is the energy, mono or stereo");
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
            pv[P::enGain] = 4;
            expectWithinAbsoluteError (run (0.2, 0.1f), 0.4f / std::sqrt (2.0f), 0.02f);
            expectWithinAbsoluteError (run (0.2, 0.5f), 1.0f, 0.01f);   // over-amplified, clamped
        }
    }
};

static EngineTests engineTests;
