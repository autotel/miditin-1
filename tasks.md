# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Tones field drops keystrokes [sonnet]

Symptom: click TONES, caret shows, typing sometimes goes nowhere -- intermittent, mostly the first
click after touching another window. Rung 3 of the known ladder is already in (`setWantsKeyboardFocus
(true)` on the editor); rung 4 is missing: the X11 FocusIn the click provokes makes
`ComponentPeer::handleFocusGain()` restore focus to the last-focused component, after the click
already focused the field. Not rung 2: `syncControls()` leaves `tones` alone while it has focus and
`layoutControls()` only toggles its visibility on a GENERATE kind change.

Port mepSynth's fix verbatim (`../mepSynth-plugin`, commit c4a5ba0): `Source/UI/FocusGuard.h`
(`focusIsWithin`, the container itself counts) + `Tests/FocusGuardTests.cpp`; in `MiditinEditor`,
`juce::FocusChangeListener`, `addMouseListener (this, true)`, `mouseDown` records the click's focus
target, `globalFocusChanged` re-asserts it within 500 ms, `reassertClickFocus (allowUnfocused)`.
Covers the librarian's fields too. Cannot be proved headlessly beyond the predicate test; note in
`wiki/history.md` that it needs the author's check in REAPER.

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
