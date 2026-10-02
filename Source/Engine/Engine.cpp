#include "Engine.h"

namespace mdt
{
namespace
{
    constexpr int outChannel = 1;
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
    if (! primed)
        return;

    if (old.voiceMode != next.voiceMode)
    {
        releaseAll (out, 0);
        for (auto& v : voices)
            v.active = false;
        if (poly())
            for (int i = 0; i < numHeld; ++i)
                claimVoice (held[(size_t) i], out, 0);
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

    if (old.run && ! next.run)
        releaseAll (out, 0);
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
        audioLevel = std::clamp ((float) std::sqrt (meanSquare), 0.0f, 1.0f);
    };

    auto it = in.begin();
    for (; nextTick < numSamples; nextTick += samplesPerTick)
    {
        const int pos = (int) nextTick;
        followAudio (pos);
        for (; it != in.end() && (*it).samplePosition <= pos; ++it)
            handle ((*it).getMessage(), out, (*it).samplePosition);

        if (! patch.run)
            continue;
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
    if (m.isNoteOn())
        inputNoteOn (m.getNoteNumber(), m.getFloatVelocity(), out, pos);
    else if (m.isNoteOff())
        inputNoteOff (m.getNoteNumber(), out, pos);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        releaseHeld (out, pos);
    else if (m.isController() && m.getControllerNumber() == patch.ccNumber)
        cc = (float) m.getControllerValue() / 127.0f;
}

void Engine::inputNoteOn (int n, float vel, juce::MidiBuffer& out, int pos)
{
    global.in.vel = vel;
    if (std::find (held.begin(), held.begin() + numHeld, n) != held.begin() + numHeld)
        return;
    if (numHeld < (int) held.size())
        held[(size_t) numHeld++] = n;
    rebuild();

    inT = t;
    global.in.note = noteToEnergy (n);
    if (patch.enRetrig && patch.enSource == EnergySource::osc)
    {
        global.lfo.ph = 0;
        global.lfoOff = clock;
        global.lfo.sh = global.rng.uniform();
    }

    Chain* target = &global;
    if (poly())
        target = &claimVoice (n, out, pos);
    else if (patch.runMode == RunMode::resetOnIn)
        global.gen.reset();
    if (patch.runMode == RunMode::resetOnIn)
        global.digital.beat = 0;

    if (patch.pulse == PulseKind::midiIn && patch.run)
        pulse (*target, global.in.vel > 0 ? global.in.vel : 0.8f, out, pos);
}

void Engine::inputNoteOff (int n, juce::MidiBuffer& out, int pos)
{
    global.in.vel = 0;   // spec §3: vel drops to 0 on note-off
    dropVoice (n, out, pos);
    auto end = std::remove (held.begin(), held.begin() + numHeld, n);
    numHeld = (int) (end - held.begin());
    rebuild();
}

void Engine::releaseHeld (juce::MidiBuffer& out, int pos)
{
    global.in.vel = 0;
    for (auto& v : voices)
        if (v.active)
            dropVoice (v.note, out, pos);
    numHeld = 0;
    rebuild();
}

Chain& Engine::claimVoice (int n, juce::MidiBuffer& out, int pos)
{
    dropVoice (n, out, pos);
    Chain* slot = nullptr;
    for (auto& v : voices)
        if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
    {
        // Full: the oldest voice gives way.
        slot = &*std::min_element (voices.begin(), voices.end(), [] (auto& a, auto& b) { return a.age < b.age; });
        dropVoice (slot->note, out, pos);
    }

    auto& v = *slot;
    v = Chain();
    v.active = true;
    v.note = n;
    v.age = ++ageCounter;
    v.rng = Rng (seeder.next());
    v.in = { global.in.vel, noteToEnergy (n), 0.5f };
    // A new voice picks the global LFO up where it is, unless RETRIG restarts it.
    if (! patch.enRetrig)
    {
        v.lfo.ph = global.lfo.ph;
        v.lfo.sh = global.lfo.sh;
    }
    v.lfo.lp = global.lfo.lp;
    v.lfoOff = patch.enRetrig ? clock : global.lfoOff;
    v.wheel.a = v.rng.uniform() * twoPi;
    v.digital.beat = clock;
    return v;
}

void Engine::dropVoice (int n, juce::MidiBuffer& out, int pos)
{
    for (auto& v : voices)
    {
        if (! v.active || v.note != n)
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

    forEachRunningChain ([&] (Chain& c) {
        const EnergyInputs cin { c.in.vel, c.in.note, cc, audioLevel };
        c.lfo.step (energySettings, cin, dt, clock, c.lfoOff, c.rng);
        c.energy = c.lfo.val;
        stepPulse (c, out, pos, dClock);

        // A tie holds its note only while the next step still continues it. An edit to the grid,
        // its length or the generator kind can break that; release then rather than strand it.
        if (c.melNote >= 0)
        {
            auto& s = notes[(size_t) c.melNote];
            if (s.on && s.until >= holdForever && ! c.gen.nextIsTie (patch, pv.i (P::melLength)))
                s.until = t;
        }
    });

    for (int m = 0; m < 128; ++m)
        if (notes[(size_t) m].on && t >= notes[(size_t) m].until)
            noteOff (m, out, pos);
}

void Engine::stepPulse (Chain& c, juce::MidiBuffer& out, int pos, double dClock)
{
    switch (patch.pulse)
    {
        case PulseKind::wheel:
        {
            const EnergyMod m { patch.modMode[0], pv[P::whModOffset], pv[P::whModScale] };
            const float s = c.wheel.step (m.apply (c.energy, pv[P::whSpeed], 2, 0, 3), pv.i (P::whSlices), dt);
            if (s >= 0) pulse (c, s, out, pos);
            break;
        }
        case PulseKind::shaker:
        {
            const EnergyMod m { patch.modMode[1], pv[P::shModOffset], pv[P::shModScale] };
            c.shaker.step (m.apply (c.energy, pv[P::shAgitate], 1, 0, 1.5f), pv.i (P::shBalls), pv[P::shBounce],
                           t, dt, c.rng, [&] (float s) { pulse (c, s, out, pos); });
            break;
        }
        case PulseKind::digital:
        {
            const EnergyMod m { patch.modMode[2], pv[P::dgModOffset], pv[P::dgModScale] };
            const int k = (int) std::lround (m.apply (c.energy, (float) pv.i (P::dgDiv), 4, 0, 4));
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
    if (t - c.lastPulseT < minPulseGap)
        return;
    c.lastPulseT = t;
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
    mappings[(size_t) mapNext] = { step.value, m.value_or (-1), t };
    mapNext = (mapNext + 1) % (int) mappings.size();
    if (patch.gen == GenKind::melody)
        c.melNote = m.value_or (-1);
    if (! m)
        return;

    noteOn (*m, strength, step.value, out, pos);
    if (patch.gen == GenKind::melody && c.gen.nextIsTie (patch, melLen))
        notes[(size_t) *m].until = holdForever;
}

// -- output ----------------------------------------------------------------------------------------

void Engine::noteOn (int m, float strength, int deg, juce::MidiBuffer& out, int pos)
{
    const int vel = std::clamp ((int) std::lround (1 + strength * 126), 1, 127);
    if (notes[(size_t) m].on)
        noteOff (m, out, pos);
    out.addEvent (juce::MidiMessage::noteOn (outChannel, m, (juce::uint8) vel), pos);
    notes[(size_t) m] = { true, vel, t + pv[P::gate] / 1000.0 };

    for (int i = std::min (logCount, (int) log.size() - 1); i > 0; --i)
        log[(size_t) i] = log[(size_t) i - 1];
    log[0] = { m, deg };
    logCount = std::min (logCount + 1, (int) log.size());
}

void Engine::noteOff (int m, juce::MidiBuffer& out, int pos)
{
    auto& s = notes[(size_t) m];
    if (! s.on)
        return;
    out.addEvent (juce::MidiMessage::noteOff (outChannel, m), pos);
    s.on = false;
    s.until = std::min (s.until, t);
}

void Engine::releaseAll (juce::MidiBuffer& out, int pos)
{
    for (int m = 0; m < 128; ++m)
        noteOff (m, out, pos);
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
    for (int m = 0; m < 128; ++m)
        s.notes[(size_t) m] = { notes[(size_t) m].on, notes[(size_t) m].vel, notes[(size_t) m].until };
    s.held = held;
    s.heldCount = numHeld;
    s.seq = poly() ? polySeq : monoSeq;
    s.log = log;
    s.logCount = logCount;
}
} // namespace mdt
