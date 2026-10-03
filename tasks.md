# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## CC LEARN

### 1. [sonnet] LEARN button for the CC energy source
Bullet (acf86b0): "to cc mode add a learn button".
- UI: `BoxButton` "LEARN" beside the `CC #` row (`Shell.cpp`, shown under `EnergySource::cc`
  only); click arms it ("LEARNING", `setOn (true)`), click again disarms. Arming is editor state,
  not `Patch`.
- The audio thread never writes `Patch`: the engine publishes the last CC it consumed in
  `UiSnapshot` (`lastCc`, plus `ccCount`, a counter bumped per CC) -- only CCs on our input
  channel (`ours` in `Engine::handle`), and only 0..119 (120..127 are channel-mode messages,
  `ccNumber`'s range already stops at 119).
- The editor, while armed, compares `ccCount` with the value it saw when arming; on a change it
  sets `ccNumber = lastCc` through `edit`, and disarms.
- Test: engine fills `lastCc`/`ccCount` from an input CC, ignores 120+ and other channels.
  Snapshot check of the energy panel under CC.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
