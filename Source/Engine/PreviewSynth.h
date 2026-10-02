#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

/** §7 "Sound": the prototype's preview voice -- a triangle through a velocity-opened lowpass, a
    6 ms attack settling to 45 % over 350 ms, a 50 ms release -- driven by the notes the engine
    just wrote. Monitoring only; it never touches the MIDI. */
namespace mdt
{
class PreviewSynth
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    /** Silence, for when it stops hearing the MIDI (SOUND off, bypass) and would miss note-offs. */
    void reset() { voices = {}; }

    void render (const juce::MidiBuffer& midi, juce::AudioBuffer<float>& audio)
    {
        int from = 0;
        for (const auto md : midi)
        {
            renderSpan (audio, from, md.samplePosition);
            from = md.samplePosition;
            const auto m = md.getMessage();
            if (m.isNoteOn()) start (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) stop (m.getNoteNumber());
        }
        renderSpan (audio, from, audio.getNumSamples());
    }

private:
    struct Voice
    {
        int note = -1;
        bool held = false;
        double phase = 0, inc = 0;
        float level = 0, peak = 0, lp = 0, cutoffCoef = 0, age = 0;
    };

    double sr = 44100;
    std::array<Voice, 16> voices {};

    void start (int note, float vel)
    {
        auto* v = &voices[0];
        for (auto& c : voices)
            if (c.level < v->level) v = &c;   // the quietest gives way
        *v = {};
        v->note = note;
        v->held = true;
        v->inc = 440.0 * std::pow (2.0, (note - 69) / 12.0) / sr;
        v->peak = 0.16f * (0.2f + vel * 0.8f);
        v->cutoffCoef = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * (900.0 + vel * 3200.0) / sr);
    }

    void stop (int note)
    {
        for (auto& v : voices)
            if (v.note == note) v.held = false;
    }

    void renderSpan (juce::AudioBuffer<float>& audio, int from, int to)
    {
        const float dt = (float) (1.0 / sr);
        for (auto& v : voices)
        {
            if (v.note < 0) continue;
            for (int i = from; i < to; ++i)
            {
                v.age += dt;
                const float target = ! v.held ? 0.0f
                                   : v.age < 0.006f ? v.peak * v.age / 0.006f
                                                    : v.peak * (0.45f + 0.55f * std::exp (-(v.age - 0.006f) / 0.12f));
                v.level = v.held && v.age < 0.006f ? target : v.level + (target - v.level) * (1.0f - std::exp (-dt / (v.held ? 0.005f : 0.05f)));
                const float tri = (float) (v.phase < 0.5 ? 4 * v.phase - 1 : 3 - 4 * v.phase);
                v.phase += v.inc;
                v.phase -= std::floor (v.phase);
                v.lp += (tri - v.lp) * v.cutoffCoef;
                const float s = v.lp * v.level;
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    audio.addSample (ch, i, s);
            }
            if (! v.held && v.level < 1e-5f) v.note = -1;
        }
    }
};
} // namespace mdt
