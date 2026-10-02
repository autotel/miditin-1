# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Shaker (author, 2026-10-02)

- [opus] **Shaker moved by the energy curve, two modes to choose** (`Patch::shakeMode`, segmented
  `SHAKE` / `FOLLOW` in the shaker's controls; structural, so Patch). The per-ball random walk
  goes. Balls live in the box frame: gravity plus minus the box's acceleration, applied as the
  change of box velocity each tick, so every ball feels the same push. `v` = the modulated
  agitation (energy through the shaker's OFFSET/SCALE, as now).
  - `SHAKE` (default; spec "energy = agitation"): the box oscillates like a hand, ~4 Hz across and
    a slower vertical component, amplitude from `v`; balls slosh together.
  - `FOLLOW`: the box rides the curve -- vertical position from `v`, smoothed ~20 ms so a square
    edge or S&H jump is a fast move, not an infinite one. Slow sine stirs little, edges jolt.
  - SHAKE button: one coherent jolt of the box (same push for every ball).
  - Display: the box drawn at its offset (scaled for visibility, a few px), snapshot carries it.
  - Tests: deterministic as before; SHAKE with v = 0 settles to no pulses, v = 1 pulses; FOLLOW
    with constant energy settles to none, a square LFO pulses near its edges.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
