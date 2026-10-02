# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Pulses (author, 2026-10-02)

- [opus] **Fast shaker misses collisions** (e.g. a 3 Hz square). The prototype's 4 /s speed cap,
  relative to the box in the box-frame physics, holds balls to a fast-moving box; raise it (40 /s
  cannot tunnel at 1 kHz), shorten the 60 ms hit cooldown, blend bounces through the wall, trail
  vertex per bounce, 24-hit flash ring. Test: POSITION 4 Hz square gives > 200 hits in 6 s.
- [opus] **Let pulses burst.** Drop `minPulseGap` (30 ms per chain: it discarded every hit of a
  shaker burst after the first). The wheel pulses once per peg crossed, not once per tick, so a
  jump over several pegs sends them all. Tests: a wheel jump of 5 pegs in one tick = 5 pulses; two
  balls hitting in the same ms = 2 notes.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
