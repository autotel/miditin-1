# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Wheel energy mode (author, 2026-10-02)

- [opus] **Wheel SPEED / ANGLE replaces SCALE/OFFSET everywhere.** Drop `ModMode`,
  `Patch::modMode`, its XML and segmented control; `EnergyMod` is `core * (offset + scale*energy)`
  (the old SCALE mode, its two sliders kept). New `Patch::wheelMode`: SPEED (energy is spin force,
  as now) or ANGLE (the wheel is sprung, critically damped, to `2pi * SPEED * m` -- SPEED reads as
  turns per energy sweep -- so pegs pass only while energy moves; a switch re-bases the angle, no
  jump). One `ENERGY` segmented row per generator: wheel SPEED/ANGLE, shaker STRENGTH/POSITION
  (moved off the SHAKE row), digital none. Tests: ANGLE with constant energy settles to no
  pulses, a ramp of one turn passes `slices` pegs, a switch fires nothing. Revert the
  centred-offset history entry to say why the switch went.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
