# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Build info in the window [sonnet]

Commit SHA and build date readable from the plugin window. A CMake script run on every build
(custom target the plugin and tests depend on) writes `build/generated/BuildInfo.cpp` only when its
content changes: short SHA with `-dirty` when the tree has uncommitted changes, `unknown` without
git, and the local build time to the minute -- so a no-op build stays a no-op within the minute.
`Source/Model/BuildInfo.h` declares `mdt::buildSha()` / `mdt::buildDate()`; core sources, so a
test asserts both are non-empty and the date parses as `YYYY-MM-DD HH:MM`.
UI: hovering the "miditin 1" title swaps the header caption for `build <sha> · <date>`; clicking
the title copies `miditin 1 <sha> <date>` to the clipboard (for bug reports). Note it in the wiki.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
