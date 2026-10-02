# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Energy and velocity (author, 2026-10-02)

- [sonnet] **Square LFO.** `Shape::square` between `saw` and `sh` (state saves names, so order is
  free): `p < 0.5 ? 1 : 0`. Patch name `"square"`, segmented `SIN TRI SAW SQR S&H`. Test: high
  for the first half-cycle, low for the second.
- [opus] **Velocity module** (fills spec §9 "output velocity design"). Three host parameters in the
  registry, 0..1, labels `ENERGY` / `PULSE` / `INPUT`, defaults 0 / 1 / 0 so today's MIDI is
  unchanged. Output velocity = clamp(energy·E + strength·P + inputVel·I, 0..1) → 1..127, where
  energy is the chain's at the pulse, strength the pulse generator's (wheel spin, ball speed,
  digital row, input velocity under MIDI-in), inputVel the chain's input note velocity -- poly:
  the voice's note; mono: the most recent still-held note (keep a velocity beside each `held`
  entry; `global.in.vel` drops to 0 on any note-off, which is right for energy, wrong here).
  UI: a `Knob` control in `Controls` -- outline circle plus an angle indicator line, nothing else,
  drag vertical, double-click to default, value readout on hover/drag like `RangeRow`. A
  `VELOCITY` row of three labelled knobs in the free space under the OUT module (right half,
  below GATE / log); grow `Shell::height` if it does not fit, and check with `MiditinSnap`.
  Tests: defaults reproduce the strength velocity; I alone gives the input velocity (mono, after
  releasing a later note too); E alone follows energy; the sum clamps at 127.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
