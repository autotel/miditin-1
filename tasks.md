# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Energy

- [sonnet] AUDIO source GAIN (over-amplification). New host param `en_gain` "Energy audio gain",
  label GAIN, 1..100 (x1..x100, +40 dB), default 1, log curve, `Fmt::times`, in
  `Source/Model/ParamRegistry.{h,cpp}` after `enHpfZero`. Engine: the AUDIO level fed to
  `EnergyInputs` is `clamp(rms * gain, 0, 1)` (Engine.cpp, where `audioLevel` is computed). UI: the
  energy canvas's bent slider (the rate arc atop the drawing, Views.cpp `drawEnergy`, Shell.cpp
  `energyZone` / drag) shows and drags GAIN when the source is AUDIO, as it does RATE/CYCLE for OSC;
  log position `log(g)/log(100)`. One test: a quiet steady input reads gain-times louder, clamped
  at 1.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
