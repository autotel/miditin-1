# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Layout, melody, animation (author, 2026-10-02)

- [sonnet] **GATE + VELOCITY above the OUT keyboard.** Move both rows from under OUT into the right
  half (GENERATE / TRANSFORM columns), between those columns' controls and the IN/OUT band; the
  OUT title and keyboard sit below them. Shorten the GENERATE and TRANSFORM control stacks'
  reserved height so the rows fit without growing the window. Recent-notes log stays under OUT.
  Check with `MiditinSnap`.
- [opus] **Smoother animation.** Measure first: paint time per frame (snapshot tool, timed), the
  editor timer's real rate, and snapshot cadence (one per block -- at 1024+ samples that is under
  50 Hz). Then fix the dominant cause: e.g. timer driven by `juce::VBlankAttachment`, cheaper
  repaint regions, drop the IN transparency layer for a precomputed colour, or interpolate
  positions between snapshots. Record the numbers in `wiki/architecture.md`.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
