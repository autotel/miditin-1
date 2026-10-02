# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Window (author, 2026-10-02)

- [sonnet] **Resizable window.** Fixed 3:2 aspect, 75 %..250 % of 900 x 600, corner resizer. The
  shell keeps its 900 x 600 layout; one `AffineTransform::scale` carries it to the window
  (mepSynth's `resized()`, minus its variable width). The width lives in the processor beside
  `openPresetName` and in the saved state, so a reopened editor comes back at its size. Check the
  snapshot tool still renders at 1x.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
