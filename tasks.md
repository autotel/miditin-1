# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Energy modulation (author, 2026-10-02)

- [opus] **Offset mode centred, wheel range halved.** Offset mode added `(off + scl*energy)*range`
  with energy 0..1, so it only ever sped things up -- the wheel at +0.3..1.7 rev/s over SPEED.
  Offset mode now uses `energy - 0.5` (the slider is the centre of the swing; scale mode keeps
  `energy`), all three generators; wheel range 2 -> 1 rev/s. Test: offset mode at energy 0.5
  gives the slider's value; wheel default swing is +-0.5 rev/s. Deviation from spec §4 ->
  `wiki/history.md`.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
