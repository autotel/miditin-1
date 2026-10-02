# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## MIDI-in source (author, 2026-10-02)

The cable bullet came back in the requirements file after it had landed (93bc50b); nothing to do.

- [opus] **MIDI-in pulse source: NOTES or CLOCK.** `Patch::midiInSource` + `Patch::clockDiv`
  (1/4 1/8 1/8T 1/16 1/16T 1/32 = 24 12 8 6 4 3 clock ticks). CLOCK counts incoming 0xF8 (0xFA
  restarts the count) and pulses every running chain on each division, at the tick's sample; VST3
  delivers no clock bytes, so while none has arrived for 0.5 s it follows the host's PPQ (playing
  only) on the same grid. Quarter-note pulses strength 1, others 0.6. Notes then only feed
  arp/transform, not pulses. Silent-until-input still holds. UI: SOURCE segmented, DIV when
  CLOCK; the MIDI-in drawing says CLOCK + div and flashes on pulses; IN header's feeds text drops
  "pulse". Tests: 0xF8 at 120 bpm, 1/8 over 2 beats = 4 pulses; host fallback 1/16 over 4 beats
  = 16; under CLOCK a note-on is no pulse.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
