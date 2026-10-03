#include "Engine.h"

namespace mdt
{
namespace
{
    constexpr float noteToEnergy (int n) { return std::clamp ((float) (n - 36) / 48.0f, 0.0f, 1.0f); }
}

void Engine::prepare (double sr)
{
    const auto keepPatch = patch;
    *this = Engine();
    patch = keepPatch;
    sampleRate = sr;
    samplesPerTick = sr / tickRate;
    msCoef = 1.0 - std::exp (-1.0 / (rmsTau * sr));
    seeder = Rng (0x2545f491u);
    global.rng = Rng (seeder.next());
    global.in = {};
    rebuild();
}

int Engine::activeVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices) n += v.active;
    return n;
}

int Engine::sounding() const noexcept
{
    int n = 0;
    for (auto& s : notes) n += s.on;
    return n;
}

uint16_t Engine::soundingChannels (int m) const noexcept
{
    uint16_t bits = 0;
    for (int ch = 1; ch <= 16; ++ch)
        if (notes[(size_t) slot (ch, m)].on)
            bits |= (uint16_t) (1 << (ch - 1));
    return bits;
}

template <typename Fn>
void Engine::forEachRunningChain (Fn&& fn)
{
    if (! poly())
        fn (global);
    else
        for (auto& v : voices)
            if (v.active)
                fn (v);
}

// -- structure -----------------------------------------------------------------------------------

void Engine::rebuild()
{
    monoSeq = buildSequence (patch, false, held.data(), numHeld);
    polySeq = buildSequence (patch, true, held.data(), numHeld);

    TransformConfig c;
    c.mode = patch.tfMode;
    c.root = patch.root;
    c.scale = patch.scale;
    c.pcs = patch.pcs;
    c.octave = pv[P::tfOctave];
    c.period = pv.i (P::tfOctLen);
    c.wrap = patch.wrap;
    pool = Pool::make (c, held.data(), numHeld);
}

void Engine::applyPatch (const Patch& next, juce::MidiBuffer& out)
{
    const Patch old = patch;
    patch = next;
    // Under VEL, no note is energy 0: the filters start there, so a note rises out of zero.
    if (next.enSource == EnergySource::vel && (old.enSource != next.enSource || ! primed))
        global.lfo.lp = 0;
    if (! primed)
        return;

    if (old.voiceMode != next.voiceMode)
    {
        releaseAll (out, 0);
        for (auto& v : voices)
            v.active = false;
        if (poly())   // the held notes go on: under LEGATO their voices join the running LFO and clock
            for (int i = 0; i < numHeld; ++i)
                claimVoice (held[(size_t) i], heldCh[(size_t) i], ! patch.legato, out, 0);
    }

    // The old channel's note-offs would now pass through: let go of everything first.
    if (old.inChannel != next.inChannel)
    {
        releaseHeld (out, 0);
        releaseAll (out, 0);
    }

    if (old.gen != next.gen || old.order != next.order || old.pulse != next.pulse)
    {
        global.gen.reset();
        for (auto& v : voices) v.gen.reset();
    }

    // Turning SYNC on keeps the cycle where it was rather than jumping to the bar.
    if (next.enSync && ! old.enSync)
    {
        const double cycle = syncBeats[std::clamp (pv.i (P::enDiv), 0, 7)];
        global.lfoOff = clock - global.lfo.ph * cycle;
        for (auto& v : voices) v.lfoOff = clock - v.lfo.ph * cycle;
    }
}

// -- block -----------------------------------------------------------------------------------------

void Engine::process (const Patch& nextPatch, const ParamValues& values, const Transport& tr,
                      const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                      const float* const* audio, int audioChannels)
{
    pv = values;
    applyPatch (nextPatch, out);
    primed = true;

    energySettings.source = patch.enSource;
    energySettings.shape = patch.enShape;
    energySettings.filter = patch.enFilter;
    energySettings.edge = patch.enEdge;
    energySettings.sync = patch.enSync;
    energySettings.div = pv.i (P::enDiv);
    energySettings.rateHz = pv[P::enRate];
    energySettings.center = pv[P::enCenter];
    energySettings.depth = pv[P::enDepth];
    energySettings.filterAmt = pv[P::enFilterAmt];
    energySettings.hpfZero = pv[P::enHpfZero];
    rebuild();

    bpm = tr.bpm > 0 ? tr.bpm : (double) pv[P::bpm];
    const bool playing = tr.known && tr.playing;
    if (wasPlaying && ! playing)
        releaseAll (out, 0);
    if (playing && ! wasPlaying)
    {
        // Lock to the host: the bar the host is in is the bar the grid shows.
        clock = prevClock = tr.ppq;
        global.digital.beat = clock;
        for (auto& v : voices) v.digital.beat = clock;
    }
    wasPlaying = playing;

    // Input RMS, per sample, read by the AUDIO energy source at each tick.
    int audioPos = 0;
    auto followAudio = [&] (int upTo) {
        for (; audioPos < upTo; ++audioPos)
        {
            float sq = 0;
            for (int ch = 0; ch < audioChannels; ++ch)
                sq += audio[ch][audioPos] * audio[ch][audioPos];
            meanSquare += ((audioChannels > 0 ? sq / (float) audioChannels : 0.0f) - meanSquare) * msCoef;
        }
        if (! std::isfinite (meanSquare))   // one NaN/inf sample from the host would stick forever
            meanSquare = 0;
        audioLevel = std::clamp ((float) std::sqrt (meanSquare) * pv[P::enGain], 0.0f, 1.0f);
    };

    auto it = in.begin();
    for (; nextTick < numSamples; nextTick += samplesPerTick)
    {
        const int pos = (int) nextTick;
        followAudio (pos);
        for (; it != in.end() && (*it).samplePosition <= pos; ++it)
            handle ((*it).getMessage(), out, (*it).samplePosition);

        clock = playing ? tr.ppq + (nextTick / sampleRate) * bpm / 60.0 : clock + dt * bpm / 60.0;
        const double dClock = clock - prevClock;
        prevClock = clock;
        tick (out, pos, dClock);
    }
    for (; it != in.end(); ++it)
        handle ((*it).getMessage(), out, (*it).samplePosition);
    followAudio (numSamples);
    nextTick -= numSamples;
    ++serial;
}

// -- input -----------------------------------------------------------------------------------------

void Engine::handle (const juce::MidiMessage& m, juce::MidiBuffer& out, int pos)
{
    // Notes are consumed; everything else (CCs, bend, pressure, programs, sysex) passes through,
    // and so does all of another channel than CH. System messages have none (channel 0).
    const int ch = m.getChannel();
    const bool ours = patch.inChannel == 0 || ch == 0 || ch == patch.inChannel;
    if (! m.isNoteOnOrOff() || ! ours)
        out.addEvent (m, pos);
    if (! ours)
        return;

    if (m.isNoteOn())
        inputNoteOn (m.getNoteNumber(), m.getFloatVelocity(), ch, out, pos);
    else if (m.isNoteOff())
        inputNoteOff (m.getNoteNumber(), ch, out, pos);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        releaseHeld (out, pos);
    else if (m.isController())
    {
        const int num = m.getControllerNumber();
        if (num == patch.ccNumber)
            cc = (float) m.getControllerValue() / 127.0f;
        if (num < 120)   // 120..127 are channel-mode messages, never a CC source
        {
            // Move to front: a busy CC keeps its one place, so it cannot crowd the others out.
            int i = 0;
            while (i < recentCcCount && recentCc[(size_t) i] != num) ++i;
            if (i == recentCcCount && recentCcCount < (int) recentCc.size()) ++recentCcCount;
            for (int k = std::min (i, (int) recentCc.size() - 1); k > 0; --k)
                recentCc[(size_t) k] = recentCc[(size_t) k - 1];
            recentCc[0] = num;
        }
    }
    else if (m.isMidiStart())
        clockCount = 0;
    else if (m.isMidiClock())
    {
        lastClockT = t;
        const int tick = clockCount++, div = clockDivTicks[std::clamp (patch.clockDiv, 0, 5)];
        if (noteClock())
        {
            // Each chain counts from its own note; the note's pulse was tick 0, this is tick 1 on.
            if (poly()) ++global.clockTicks;   // what a legato voice inherits
            forEachRunningChain ([&] (Chain& c) {
                if (++c.clockTicks % div == 0)
                    pulse (c, c.clockTicks % 24 == 0 ? 1.0f : 0.6f, out, pos);
            });
        }
        else if (clockPulses() && tick % div == 0)
            forEachRunningChain ([&] (Chain& c) { pulse (c, tick % 24 == 0 ? 1.0f : 0.6f, out, pos); });
    }
}

void Engine::inputNoteOn (int n, float vel, int ch, juce::MidiBuffer& out, int pos)
{
    global.in.vel = vel;
    for (int i = 0; i < numHeld; ++i)
        if (heldMatches (i, n, ch))
            return;
    // LEGATO: a note under a held note continues what is running; only a phrase's first restarts.
    const bool restart = ! patch.legato || numHeld == 0;
    if (numHeld < (int) held.size())
    {
        heldVel[(size_t) numHeld] = vel;
        heldCh[(size_t) numHeld] = ch;
        held[(size_t) numHeld++] = n;
    }
    rebuild();

    inT = t;
    global.in.note = noteToEnergy (n);
    if (restart && patch.enRetrig && patch.enSource == EnergySource::osc)
    {
        restartLfo (global);
        global.lfo.sh = global.rng.uniform();
    }

    Chain* target = &global;
    if (poly())
        target = &claimVoice (n, ch, restart, out, pos);
    else if (patch.runMode == RunMode::resetOnIn)
        global.gen.reset();
    if (patch.runMode == RunMode::resetOnIn)
        global.digital.beat = 0;

    if (patch.pulse == PulseKind::midiIn && patch.midiInSource == MidiInSource::notes)
        pulse (*target, global.in.vel > 0 ? global.in.vel : 0.8f, out, pos);
    else if (noteClock() && restart)
    {
        restartClock (global);
        restartClock (*target);
        pulse (*target, 1.0f, out, pos);
    }
}

bool Engine::heldMatches (int i, int n, int ch) const noexcept
{
    return held[(size_t) i] == n && (! poly() || heldCh[(size_t) i] == ch);
}

void Engine::inputNoteOff (int n, int ch, juce::MidiBuffer& out, int pos)
{
    global.in.vel = 0;   // spec §3: vel drops to 0 on note-off
    dropVoice (n, ch, out, pos);
    int kept = 0;
    for (int i = 0; i < numHeld; ++i)
        if (! heldMatches (i, n, ch))
        {
            heldVel[(size_t) kept] = heldVel[(size_t) i];
            heldCh[(size_t) kept] = heldCh[(size_t) i];
            held[(size_t) kept++] = held[(size_t) i];
        }
    numHeld = kept;
    rebuild();
}

void Engine::releaseHeld (juce::MidiBuffer& out, int pos)
{
    global.in.vel = 0;
    for (auto& v : voices)
        if (v.active)
            dropVoice (v.note, v.ch, out, pos);
    numHeld = 0;
    rebuild();
}

Chain& Engine::claimVoice (int n, int ch, bool restart, juce::MidiBuffer& out, int pos)
{
    dropVoice (n, ch, out, pos);
    Chain* spot = nullptr;
    for (auto& v : voices)
        if (! v.active) { spot = &v; break; }
    if (spot == nullptr)
    {
        // Full: the oldest voice gives way.
        spot = &*std::min_element (voices.begin(), voices.end(), [] (auto& a, auto& b) { return a.age < b.age; });
        dropVoice (spot->note, spot->ch, out, pos);
    }

    auto& v = *spot;
    v = Chain();
    v.active = true;
    v.note = n;
    v.ch = ch;
    v.age = ++ageCounter;
    v.rng = Rng (seeder.next());
    v.in = { global.in.vel, noteToEnergy (n), 0.5f };
    // A new voice picks the global LFO and note clock up where they are, unless it restarts them.
    const bool retrig = patch.enRetrig && restart;
    if (! retrig)
    {
        v.lfo.ph = global.lfo.ph;
        v.lfo.sh = global.lfo.sh;
    }
    if (! restart)
    {
        v.clockTicks = global.clockTicks;
        v.clockBeats = global.clockBeats;
    }
    // Under VEL the voice's own note was off until now, energy 0: its filters rise from there.
    v.lfo.lp = patch.enSource == EnergySource::vel ? 0.0f : global.lfo.lp;
    v.lfoOff = global.lfoOff;
    if (retrig)
        restartLfo (v);
    v.wheel.a = v.rng.uniform() * twoPi;
    v.digital.beat = clock;
    return v;
}

void Engine::restartLfo (Chain& c)
{
    c.lfo.ph = pv[P::enPhase];
    c.lfoOff = clock - c.lfo.ph * syncBeats[std::clamp (pv.i (P::enDiv), 0, 7)];
}

void Engine::dropVoice (int n, int ch, juce::MidiBuffer& out, int pos)
{
    for (auto& v : voices)
    {
        if (! v.active || v.note != n || v.ch != ch)
            continue;
        if (v.melNote >= 0 && notes[(size_t) v.melNote].on && notes[(size_t) v.melNote].until >= holdForever)
            noteOff (v.melNote, out, pos);
        v.active = false;
    }
}

void Engine::kick()
{
    forEachRunningChain ([] (Chain& c) {
        c.wheel.kick();
        c.shaker.kick (c.rng);
    });
}

// -- the tick --------------------------------------------------------------------------------------

void Engine::tick (juce::MidiBuffer& out, int pos, double dClock)
{
    t += dt;

    // In poly the global LFO keeps running: new voices inherit its phase, and the display shows it
    // when no voice is held.
    if (poly())
    {
        const EnergyInputs gin { global.in.vel, global.in.note, cc, audioLevel };
        global.lfo.step (energySettings, gin, dt, clock, global.lfoOff, global.rng);
        global.energy = global.lfo.val;
    }

    // CLOCK with no 0xF8 arriving (VST3 delivers none): the host's transport is the clock. A grid
    // point g pulses when it falls in [prev, now) -- so beat 0 at a transport start counts once.
    // START = NOTE needs no transport position: each chain counts the engine's beats from its note,
    // so it runs with the host stopped too.
    const bool noClockBytes = clockPulses() && t - lastClockT > 0.5 && dClock > 0 && dClock < 0.5;
    if (noClockBytes && noteClock())
    {
        const int k = 24 / clockDivTicks[std::clamp (patch.clockDiv, 0, 5)];   // cells a beat
        auto count = [&] (Chain& c) {
            const auto before = (long long) std::floor (c.clockBeats * k);
            c.clockBeats += dClock;
            const auto cell = (long long) std::floor (c.clockBeats * k);
            return cell > before ? cell : 0;
        };
        if (poly()) count (global);   // what a legato voice inherits
        forEachRunningChain ([&] (Chain& c) {
            if (const auto cell = count (c))
                pulse (c, cell % k == 0 ? 1.0f : 0.6f, out, pos);
        });
    }
    else if (noClockBytes && wasPlaying)
    {
        const double k = 24.0 / clockDivTicks[std::clamp (patch.clockDiv, 0, 5)];   // cells a beat
        if (const double g = std::ceil ((clock - dClock) * k); g < clock * k)
        {
            const float strength = std::fmod (g, k) < 0.5 ? 1.0f : 0.6f;   // on the beat
            forEachRunningChain ([&] (Chain& c) { pulse (c, strength, out, pos); });
        }
    }

    forEachRunningChain ([&] (Chain& c) {
        const EnergyInputs cin { c.in.vel, c.in.note, cc, audioLevel };
        c.lfo.step (energySettings, cin, dt, clock, c.lfoOff, c.rng);
        c.energy = c.lfo.val;
        stepPulse (c, out, pos, dClock);

        // A tie holds its note only while the next step still continues it. An edit to the grid,
        // its length or the generator kind can break that; so does lifting the last key, which
        // stops a self-running pulse -- under MIDI-in the next key press is the next step.
        if (c.melNote >= 0)
        {
            auto& s = notes[(size_t) c.melNote];
            const bool stopped = numHeld == 0 && patch.pulse != PulseKind::midiIn;
            if (s.on && s.until >= holdForever && (stopped || ! c.gen.nextIsTie (patch, pv.i (P::melLength))))
                s.until = t;
        }
    });

    for (int k = 0; k < (int) notes.size(); ++k)
        if (notes[(size_t) k].on && t >= notes[(size_t) k].until)
            noteOff (k, out, pos);
}

void Engine::stepPulse (Chain& c, juce::MidiBuffer& out, int pos, double dClock)
{
    switch (patch.pulse)
    {
        case PulseKind::wheel:
        {
            const EnergyMod m { pv[P::whModOffset], pv[P::whModScale] };
            const int pegs = c.wheel.step (m.apply (c.energy, pv[P::whSpeed], 0, 3), patch.wheelMode, pv.i (P::whSlices), dt);
            for (int k = 0; k < pegs; ++k)
                pulse (c, c.wheel.strength, out, pos);
            break;
        }
        case PulseKind::shaker:
        {
            const EnergyMod m { pv[P::shModOffset], pv[P::shModScale] };
            c.shaker.step (m.apply (c.energy, pv[P::shAgitate], 0, 1.5f), patch.shakeMode, pv.i (P::shBalls), pv[P::shBounce],
                           t, dt, c.rng, [&] (float s) { pulse (c, s, out, pos); });
            break;
        }
        case PulseKind::digital:
        {
            const EnergyMod m { pv[P::dgModOffset], pv[P::dgModScale] };
            const int k = (int) std::lround (m.apply (c.energy, (float) pv.i (P::dgDiv), 0, 4));
            const float s = c.digital.step (dClock, patch.digBase, k, pv[P::dgSwing]);
            if (s >= 0)
            {
                c.digital.lastPulseT = t;
                pulse (c, s, out, pos);
            }
            break;
        }
        case PulseKind::midiIn:
            break;
    }
}

void Engine::pulse (Chain& c, float strength, juce::MidiBuffer& out, int pos)
{
    // A MIDI processor: nothing comes out until something comes in. The physics keep moving for
    // the display; only the generator waits. Poly voices exist only while their note is held.
    if (numHeld == 0)
        return;
    pulseT = genT = t;
    pulseV = strength;

    const int melLen = pv.i (P::melLength);
    const auto step = c.gen.advance (patch, poly() ? polySeq : monoSeq, melLen, c.rng);

    if (step.kind == GenStep::tie)
    {
        if (c.melNote >= 0 && notes[(size_t) c.melNote].on)
            notes[(size_t) c.melNote].until = c.gen.nextIsTie (patch, melLen) ? holdForever : t + pv[P::gate] / 1000.0;
        return;
    }
    if (step.kind == GenStep::rest)
    {
        c.hasDeg = false;
        c.melNote = -1;
        return;
    }

    c.hasDeg = true;
    c.curDeg = step.value;

    // Tone 0 lands on the voice's own note in poly; TRANSPOSE BY IN does the same for the melody,
    // in mono from the most recently pressed held note.
    std::optional<int> ref;
    if (patch.gen == GenKind::melody)
    {
        if (patch.melTranspose && (poly() || numHeld > 0))
            ref = poly() ? c.note : held[(size_t) numHeld - 1];
    }
    else if (poly())
    {
        ref = c.note;
    }

    const auto m = pool.transform (step.value, ref);
    // Out on CH; under OMNI poly keeps each voice's channel, mono uses OUT.
    const int ch = poly() ? c.ch : patch.inChannel > 0 ? patch.inChannel : patch.outChannel;
    mappings[(size_t) mapNext] = { step.value, m.value_or (-1), t };
    mapNext = (mapNext + 1) % (int) mappings.size();
    if (patch.gen == GenKind::melody)
        c.melNote = m ? slot (ch, *m) : -1;
    if (! m)
        return;

    // VELOCITY: energy, pulse strength and the input note's velocity, each by its amount. Poly:
    // the voice's own note; mono: the latest still held (in.vel drops on any note-off).
    const float inVel = poly() ? c.in.vel : heldVel[(size_t) numHeld - 1];
    const float level = c.energy * pv[P::velEnergy] + strength * pv[P::velPulse] + inVel * pv[P::velInput];
    noteOn (slot (ch, *m), level, step.value, out, pos);
    if (patch.gen == GenKind::melody && c.gen.nextIsTie (patch, melLen))
        notes[(size_t) slot (ch, *m)].until = holdForever;
}

// -- output ----------------------------------------------------------------------------------------

void Engine::noteOn (int k, float level, int deg, juce::MidiBuffer& out, int pos)
{
    const int vel = std::clamp ((int) std::lround (1 + level * 126), 1, 127), m = k % 128;
    if (notes[(size_t) k].on)
        noteOff (k, out, pos);
    out.addEvent (juce::MidiMessage::noteOn (k / 128 + 1, m, (juce::uint8) vel), pos);
    notes[(size_t) k] = { true, vel, t + pv[P::gate] / 1000.0 };

    for (int i = std::min (logCount, (int) log.size() - 1); i > 0; --i)
        log[(size_t) i] = log[(size_t) i - 1];
    log[0] = { m, deg };
    logCount = std::min (logCount + 1, (int) log.size());
}

void Engine::noteOff (int k, juce::MidiBuffer& out, int pos)
{
    auto& s = notes[(size_t) k];
    if (! s.on)
        return;
    out.addEvent (juce::MidiMessage::noteOff (k / 128 + 1, k % 128), pos);
    s.on = false;
    s.until = std::min (s.until, t);
}

void Engine::releaseAll (juce::MidiBuffer& out, int pos)
{
    for (int k = 0; k < (int) notes.size(); ++k)
        noteOff (k, out, pos);
    global.melNote = -1;
    for (auto& v : voices) v.melNote = -1;
}

// -- snapshot --------------------------------------------------------------------------------------

void Engine::fillSnapshot (UiSnapshot& s) const
{
    s.serial = serial;
    s.t = t;
    s.clock = clock;
    s.poly = poly();
    s.cc = cc;
    s.recentCc = recentCc;
    s.recentCcCount = recentCcCount;

    auto view = [] (const Chain& c) {
        UiSnapshot::View v;
        v.note = c.note;
        v.energy = c.lfo.val;
        v.energyRaw = c.lfo.raw;
        v.wheelA = c.wheel.a;
        v.wheelW = c.wheel.w;
        v.flick = c.wheel.flick;
        v.ballCount = c.shaker.count;
        v.balls = c.shaker.balls;
        v.walls = c.shaker.walls;
        v.effort = c.shaker.effort;
        v.boxX = c.shaker.boxX;
        v.boxY = c.shaker.boxY;
        v.beat = c.digital.beat;
        v.digDiv = c.digital.div;
        v.digT = c.digital.lastPulseT;
        v.arpI = c.gen.arpI;
        v.arpPrev = c.gen.arpPrev;
        v.melI = c.gen.melI;
        return v;
    };

    s.numViews = 0;
    float maxEnergy = 0;
    if (poly())
        for (auto& c : voices)
            if (c.active)
            {
                s.views[(size_t) s.numViews++] = view (c);
                maxEnergy = std::max (maxEnergy, c.lfo.val);
            }
    // The global chain shows when nothing else does, and always leads the energy plot in mono.
    if (s.numViews == 0)
    {
        s.views[0] = view (global);
        s.numViews = 1;
        maxEnergy = global.lfo.val;
    }
    s.energy = maxEnergy;

    s.pulseT = pulseT;
    s.pulseV = pulseV;
    s.genT = genT;
    s.inT = inT;

    const Chain* lead = poly() ? nullptr : &global;
    if (lead == nullptr)
        for (auto& c : voices)
            if (c.active) { lead = &c; break; }
    s.hasDeg = lead != nullptr && lead->hasDeg;
    s.curDeg = lead != nullptr ? lead->curDeg : 0;

    s.mappings = mappings;
    s.mapNext = mapNext;
    // By pitch for the OUT keyboard: one sounding on any channel, else the latest to end.
    s.notes = {};
    for (size_t k = 0; k < notes.size(); ++k)
        if (auto& d = s.notes[k % 128]; notes[k].on > d.on || (notes[k].on == d.on && notes[k].until > d.until))
            d = notes[k];
    s.held = held;
    s.heldCount = numHeld;
    s.seq = poly() ? polySeq : monoSeq;
    s.log = log;
    s.logCount = logCount;
}
} // namespace mdt
