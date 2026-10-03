# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Round shaker, bigger window

### 1. [opus] Round shaker
Bullet: the POSITION x sway (16f786e) is no good either; make the shaker round instead. Revert
the sway. The container becomes a circle (centre 0.5, radius 0.5 in box units): a ball past
`0.5 - radius` is put back on the rim and its outward normal velocity reflected with `bounce`;
the outward speed is the hit's strength. `WallHit` keeps `wall = 'o'`, `pos` = the hit's angle.
`blend` mirrors a bounced ball across the rim's tangent (the square's wall mirroring, round).
`drawShaker` draws a circle and its flashes as rim arcs. Spec §4 says box: note in history.

### 2. [sonnet] Default window 130%
Bullet: the default window should be about 130% of today's (900 wide) -- it starts too small.
`editorWidth` default and the state-load fallback 900 → 1170 (`PluginProcessor.{h,cpp}`); the
resize limits already allow it. A saved project keeps its own width.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
