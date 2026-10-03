# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Shaker POSITION sway

### 1. [opus] Untilt the shaker; POSITION also moves the box on x
Bullet: the tilt (8ca7b6a) did not work -- revert it. In POSITION (`ShakeMode::follow`) the box
rides energy on y only; give it an x component too, as STRENGTH (`shake`) has. x must not be a
linear function of y (that is the tilt again): `x = sway * sin (2 pi (energy - 0.5))`, eased like
y, so the push direction turns as energy moves and balls cannot keep a lane.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
