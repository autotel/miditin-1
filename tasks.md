# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## MIDI-in pulse and note restarts

Filed in bd95120; tasks 1-2 landed. `restartClock` / `Chain::clockTicks|clockBeats` are task 2's origin.

### 3. [opus] LEGATO: a note under a held note continues the LFO and the clock
Bullet: when a note starts while another is still held, the LFO restart (RETRIG) and the clock
restart (task 2's START = NOTE) either happen anyway or are skipped because a note continues.
Default: if any note is held, they continue. An option switches to restart-on-every-note.
- `Patch`: `bool legato = true` (saved/loaded; structural). A restart happens when
  `! legato || numHeld == 0` *before* the new note is added -- i.e. legato restarts only on the
  first note of a phrase. Compute it once at the top of `Engine::inputNoteOn`.
- Governs exactly: RETRIG's `restartLfo` (+ the S&H redraw) on the global chain and in
  `claimVoice`, and task 2's clock origin. Not RESET ON IN (generator reset, digital grid
  `beat = 0`): the bullet names the LFO and the clock only -- file it if wanted.
- Poly + legato + another note held: the new voice does not restart; it inherits the global
  chain's LFO phase/offset (as RETRIG OFF does today) and the global chain's clock counters.
- UI: one `BoxButton` "LEGATO ON/OFF", in the IN panel's bottom row beside RELEASE ALL (it is a
  rule about input notes, and it governs two panels). Shown only when it governs something:
  `(enSource == osc && enRetrig) || (MIDI IN && CLOCK && START = NOTE)`.
- Default changes RETRIG in mono: today every note restarts; now only a phrase's first does.
  The spec (§3 "restart the cycle on each incoming note") is not edited -- record the deviation in
  `wiki/history.md`, the rule in `wiki/architecture.md`.
- Tests: the RETRIG test keeps passing (single notes); new: mono overlapping notes -- legato keeps
  the phase across the second note-on, LEGATO OFF restarts it; a gap (all released) restarts under
  both; poly legato voice inherits the global phase; same three cases for the START = NOTE grid.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
