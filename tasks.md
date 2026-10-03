# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Editor size

### 1. [sonnet] Remember the editor size across sessions
Bullet: the editor should remember its size between DAW sessions. Today the width lives in each
instance's state, so a new instance or project starts at the default, and the width only sticks
once the project is saved. Make it one machine-wide preference instead: `editor.width` in the
editor's existing `settings` PropertiesFile, read at construction (default 1170, clamped to the
resize limits) and written in the editor's destructor (not per resize step -- the file saves on
every `setValue`). Drop `MiditinProcessor::editorWidth` and its state property.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
