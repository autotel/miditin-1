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
releases every sounding note. The engine runs whenever RUN is on, transport or not, so a stopped DAW
still plays live input.

## Chains and the voice pool

A `Chain` is one energy LFO + one pulse generator + one generator position. Mono uses the engine's
own chain. Poly claims chains from a fixed pool of `maxVoices` (16) — one per held input note,
created on note-on, released on note-off; the oldest is stolen when the pool is full. The global
chain's LFO keeps running in poly so a new voice can inherit its phase (prototype behaviour).

The transformer (`Transformer.h`) is shared and pure: `(tone, TransformConfig, held notes) → note`,
with `inverse` giving the pool index for poly voices and TRANSPOSE BY IN.

## Output notes

`NoteTable` tracks every sounding output note (one channel, 128 slots) with its release time.
Note-offs go out at gate end, on retrigger, voice removal, structural change, RUN off, transport
stop, bypass and `releaseResources`. A melody tie holds a note with no release time; each tick the
owning chain checks the tie is still valid (generator still melody, next step still a tie) and
releases it otherwise, so an edit can never strand one.

## Host parameters vs Patch

Host parameters (`ParamRegistry.cpp`) are the prototype's **sliders**: continuous performance
controls and integer knobs (slices, ball count, grid division, melody length, root, OCT LEN…).
`Patch` holds everything **chosen** rather than turned: kinds, modes, orders, toggles, scale type,
the typed arpeggio, the melody grid, the pitch-class set, the CC number. Choices reshape the chain or
are routing; automating them mid-note is how notes hang. This keeps the rule one line long.

## UI

The editor animates from the latest `UiSnapshot` at its frame rate and keeps its own short
histories (energy plot) from successive snapshots. Closing it changes no MIDI.
