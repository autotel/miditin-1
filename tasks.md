# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Shaker tilt

### 1. [opus] Tilt the shaker box a little
Bullet: rotate the shaker a bit -- otherwise the balls latch onto the same vertical positions
(FOLLOW moves the box straight up and down and gravity is straight down, so settled balls bounce
in vertical lock-step). `Shaker::tilt` (~6 deg): physics stays in the box frame with
axis-aligned walls; gravity and the box's push are rotated into that frame. `drawShaker` draws
the box, flashes, trails and balls under the same rotation, scaled to fit; the shaker's cable
anchors move with it.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
