# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Requirement filed 2026-10-02 (1 bullet → 1 task)

- [opus] **VEL energy starts from zero.** No note = velocity energy 0, so the filters see each
  note rise out of 0: LPF climbs from 0 to the velocity, HPF spikes from 0. `EnergyInputs::vel`
  defaults to 0; a poly voice under VEL starts with `lfo.lp = 0` instead of the global's; the
  mono chain starts at 0 when prepared under VEL. Test mono and poly: LPF energy just after the
  note-on is near the bottom of the band and reaches the velocity's level after a few tau.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
