# Architecture

## Threads

| Thread | Owns | Talks to the other through |
|---|---|---|
| audio (`processBlock`) | `mdt::Engine`: every chain, the clock, held input notes, sounding output notes | reads `Patch` + param atomics; writes `UiSnapshot` |
| message (editor, host state) | `Patch` (structural), the APVTS (continuous) | publishes `Patch`; posts `UiEvent`s; polls `UiSnapshot` |

Three one-way channels, all lock-free and allocation-free on the audio side:

- **Patch → audio**: `TripleBuffer<Patch>`. `Patch` is trivially copyable (fixed arrays, enums,
  a 12-bit pitch-class mask), so the message thread writes a copy and the audio thread picks up the
  latest at block start. No pointers in it, hence nothing to free — the reason mepSynth's
  swap-back exists does not apply here.
- **Params → audio**: APVTS raw atomics, read once per block into `ParamValues`.
- **UI → audio**: `UiEvents`, an `AbstractFifo` of small PODs (IN-keyboard note on/off, release
  all, spin/shake kick). The UI never touches engine state directly.
- **Audio → UI**: `TripleBuffer<UiSnapshot>`, written once per block, latest wins.

## The clock

The engine ticks at a fixed **1 kHz** control rate (`Engine::tickRate`), independent of sample rate
and block size; a fractional sample accumulator places each tick inside the block, and every
output event carries that tick's sample offset. Input MIDI is applied at the first tick at or after
its sample position. All randomness is a seeded xorshift per chain. So the same input renders the
same MIDI at any sample rate or block size, which is what the tests rely on.

Musical time (`clock`, in beats) follows host PPQ while the host is playing and free-runs at the
host tempo — or the `bpm` parameter when there is no host tempo — when it is not. A transport stop
releases every sounding note. The engine always runs, transport or not, so a stopped DAW
still plays live input.

## Chains and the voice pool

A `Chain` is one energy LFO + one pulse generator + one generator position. Mono uses the engine's
own chain. Poly claims chains from a fixed pool of `maxVoices` (16) — one per held input note,
created on note-on, released on note-off; the oldest is stolen when the pool is full. The global
chain's LFO keeps running in poly so a new voice can inherit its phase (prototype behaviour).

The transformer (`Transformer.h`) is shared and pure: `(tone, TransformConfig, held notes) → note`,
with `inverse` giving the pool index for poly voices and TRANSPOSE BY IN.

## Output notes

`notes` tracks every sounding output note by `slot (ch, note)` (16 x 128) with its release time,
so the same pitch on two channels never cuts the other short. CH (`Patch::inChannel`) picks the
input: OMNI, or one channel whose notes are played while every other channel passes through
untouched. Output is on CH; under OMNI poly voices keep their input note's channel, and mono --
whose notes trace back to no one input note -- uses OUT (`Patch::outChannel`). Poly keys held
notes and voices by (note, channel); mono by note alone. The IN keyboard plays on CH.
Note-offs go out at gate end, on retrigger, voice removal, structural change, transport
stop, bypass and `releaseResources`. A melody tie holds a note with no release time; each tick the
owning chain checks the tie is still valid (generator still melody, next step still a tie) and
releases it otherwise, so an edit can never strand one.

Bypass also forgets the held input notes: it never sees their note-offs, so a poly voice would
otherwise run on after the key is up. The preview synth is reset whenever it stops hearing the
MIDI (SOUND off, bypass) for the same reason.

## Host parameters vs Patch

Host parameters (`ParamRegistry.cpp`) are the prototype's **sliders**: continuous performance
controls and integer knobs (slices, ball count, grid division, melody length, OCT LEN…).
`Patch` holds everything **chosen** rather than turned — the prototype's segmented controls,
toggles and click targets: kinds, modes, orders, sync/retrig/wrap, scale type, root, digital base,
the typed arpeggio, the melody grid, the pitch-class set, the CC number. Choices reshape the chain or
are routing; automating them mid-note is how notes hang. This keeps the rule one line long.

## Energy additions beyond the spec

- **One-sided filter** (`Patch::enEdge`): the energy smoother snaps instead of smoothing on the
  unselected edge, so one switch serves both filters — LPF `- ONLY` = instant onset, slow release;
  HPF `+ ONLY` = only rises register.
- **HPF zero** (`en_hpf_zero`): the HPF output rests on its own level, a dashed line dragged in
  the plot, instead of on CENTER (equal at the 0.5 defaults).
- **RETRIG start phase** (`en_phase`): RETRIG restarts the LFO there, free or synced.
- **AUDIO source**: an optional mono/stereo input bus is measured, never passed through. The
  engine tracks mean square per sample (30 ms one-pole, channels averaged) and each tick reads
  `sqrt` of it times GAIN (`en_gain`, ×1..×100, clamped to 1), as the energy input — one level
  shared by every chain. GAIN rides the energy arc that RATE/CYCLE use for OSC. The one exception to
  "same MIDI at any sample rate": block-size independent, not bit-identical across rates. The
  JUCE Standalone mutes its input by default (feedback guard) — untick it in the audio settings.

## UI

The editor animates from the latest `UiSnapshot` at its frame rate and keeps its own short
histories (energy plot) from successive snapshots. Closing it changes no MIDI.
Frames come from `juce::VBlankAttachment`: on Linux the peer flushes repaints on its own vblank
timer, and a separate 60 Hz `Timer` beat against it, dropping or doubling frames. The audio thread
publishes one snapshot per block, so the display runs a block behind on its own clock and
`blend()`s the last two (wheel, balls, box, energy, beat). Measured at 60 fps, |Δstep| / mean step
of the wheel angle: block 256 0.08 → 0.03, 1024 0.60 → 0.09 (39 stalled frames in 3 s → 0),
2048 2.0 → 0.07. Painting the animated bands costs 3.5 ms a frame at 1x, 6.6 ms at 2x.
The shell is laid out once at 900 x 570; the editor resizes at the design aspect and scales the shell
with one transform. The processor keeps the width (and saves it), since the editor dies on close.

## Output velocity

The VELOCITY knobs (`vel_energy` / `vel_pulse` / `vel_input`, 0..1) weight three sources, summed
and clamped to 0..1 → 1..127: the chain's energy at the pulse, the pulse strength, and the input
note's velocity — poly the voice's own, mono the latest still-held note (`heldVel` sits beside
`held`; `in.vel` zeroes on any note-off, as the energy source wants). Defaults 0 / 1 / 0 are the
old strength-only velocity. A knob is `RangeRow` with `knob = true`.

## MIDI-in CLOCK

`MidiInSource::clock` pulses every running chain each `clockDivTicks[clockDiv]` 0xF8 ticks (0xFA
restarts the count), at the tick's sample. VST3 has no MIDI clock events -- JUCE's wrapper never
delivers 0xF8 -- so while none has arrived for 0.5 s the host's PPQ is the clock (transport
playing only), same grid; a grid point counts when it falls in `[prev, now)`, so beat 0 at a
transport start pulses once. Real clock bytes reach the Standalone from a MIDI device. Clock
bytes pass through like any non-note.

START = NOTE (`Patch::clockStart`) counts per chain from its note instead: the note-on pulses at
once (the downbeat), then `Chain::clockTicks` counts 0xF8 from 1 (0xFA ignored), or, without
0xF8, `Chain::clockBeats` accumulates the engine's guarded `dClock` -- no transport position
needed, so it runs with the host stopped too. Poly: each voice from its own note; the global
chain keeps counting so a voice can inherit it.
