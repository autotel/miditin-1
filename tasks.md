# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Pressure, bend, poly aftertouch as CC sources

### 1. [sonnet] AT / BEND / POLY AT beside the CCs
Bullet (filed just before this): channel pressure, pitch bend and poly aftertouch selectable as
the CC energy source and listed by LEARN; poly aftertouch follows each voice's own key in poly.
- `Patch::ccNumber` 0..119 a CC, then `ccPressure = 120`, `ccBend = 121`, `ccPolyAt = 122`
  (CC numbers stop at 119, so no new field). Load clamps to 0..122. `ccName (n)` in `Patch.h`:
  "CC 74" / "AT" / "BEND" / "POLY AT", used by the LEARN chips, the CC # readout and the plot's
  source label.
- Engine (`handle`, our channel only; messages still pass through): pressure /127, bend 14-bit
  /16383 (centre 0.5), poly AT /127 into a per-key `polyAt[note]` (zeroed at that key's
  note-on) and into the shared `cc` (mono follows the latest key's pressure). Poly + POLY AT: a
  voice's `EnergyInputs::cc` is `polyAt[voice.note]`. Each also enters the recent-CC list.
- Tests: each source drives `cc`; poly voices read their own key's pressure; the list holds
  120..122 when they arrive.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
