# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## MIDI processor (author, 2026-10-02)

miditin 1 takes what comes in as MIDI; sending MIDI out is its main function; the IN keyboard is a
utility. Deviates from the prototype (a self-playing page) -- record each in `wiki/history.md`.

- [sonnet] **Input-driven defaults.** `Patch` defaults `gen = input`, `tfMode = held`: a classic arp
  of the held notes, paced by the wheel. Fix tests that assumed arp/scale.
- [sonnet] **Standalone starts SOUND OFF.** Drop `p.preview = wrapperType == wrapperType_Standalone`;
  SOUND stays a toggle, still remembered in state.
- [opus] **IN keyboard secondary.** Muted relative to OUT: dimmer keys and label, OUT keeps the
  palette's emphasis. Notes from the DAW/controller still light it. Check with `MiditinSnap`.
- [sonnet] **Host check feeds MIDI in.** With defaults: nothing out while nothing held; a held
  chord gives notes only from its pitches; a CC passes through; releasing leaves nothing sounding.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; output velocity design; per-voice transformer in poly;
transform → OUT line.
