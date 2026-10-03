# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## MIDI-in pulse and note restarts

Three bullets filed in bd95120, three tasks. Do them in order: 3 builds on 2's per-chain origin.

### 1. [sonnet] No energy cable under MIDI IN
Bullet: "When pulse gen is midi in, then the energy is not connected graphically to the pulse
generator." Read as: it *should* not be -- under MIDI IN energy modulates nothing in the pulse
(`Engine::stepPulse`, `case PulseKind::midiIn: break`), yet `views::drawConnections`
(`Source/UI/Views.cpp`, the `under == cableUnder` block) still draws the LFO → pulse bezier and its
end disc to the MIDI-in circle (see `MiditinSnap` scene `midiin-clock`). Skip that block when
`f.patch->pulse == PulseKind::midiIn`; drop the now-unused `midiIn` `pTop` in `anchorsFor` only if
nothing else reads it. The pulse → generate wire, the IN dashed line under the circle and the
energy plot stay. Check with `MiditinSnap` (`midiin-clock`, `midiin-held-nowrap`).

### 2. [opus] CLOCK: START = NOTE, the count measured from the note
Bullet: in MIDI-in CLOCK mode, an option for a note's start to mark the start of the clock count;
a note can start at any time, and then the clock measures from the note's start.
- `Patch`: `ClockStart clockStart = ClockStart::clock` (enum `{ clock, note }`), saved/loaded like
  `midiInSource`; structural, not a param. UI: `Segmented` "START" `{ "CLOCK", "NOTE" }` in the
  pulse panel under DIV, shown only under MIDI IN + CLOCK (`Shell.cpp` beside `clockDiv`). The
  MIDI-in circle may show the start under the division (`drawMidiIn`), if it fits.
- START = CLOCK: today's behaviour, unchanged (0xF8 count since 0xFA; host-PPQ grid without 0xF8).
- START = NOTE: each chain carries its own count since its origin. At the origin the chain pulses
  at once (strength 1.0, the count's downbeat), then every `clockDivTicks[clockDiv]` ticks after:
  - with 0xF8 arriving: a per-chain tick counter, reset to 0 at the origin and incremented on each
    0xF8; pulse when it reaches a multiple of the division (the next 0xF8 after the note is tick 1,
    so the note's own pulse is never doubled). 0xFA does not touch it -- the note is the start.
  - without 0xF8: a per-chain beat accumulator, `+= dClock` each tick (dClock already guarded
    `0 < dClock < 0.5`, so transport jumps/loops cannot fire a burst); pulse when
    `floor (beats * cellsPerBeat)` advances. Unlike START = CLOCK this needs no transport
    position, so it runs while the host is stopped too (the engine clock free-runs at host tempo
    or BPM, `wiki/architecture.md` "The clock") -- CLOCK mode becomes usable in the Standalone.
    Accent 1.0 on whole beats since the origin, 0.6 otherwise, as today.
- Origin: the chain's note-on. Mono: the global chain, at each input note-on (task 3 limits this).
  Poly: each voice at `claimVoice`. Keep the global chain's counters advancing in poly too, so a
  voice can inherit them (task 3).
- Tests (`Tests/EngineTests.cpp`, beside "MIDI-in CLOCK"): a note at an off-grid time pulses at
  its start and every division after, with 0xF8 and without (host stopped and playing); no double
  pulse at the first 0xF8; poly voices started at different times keep their own grids; notes
  balanced. `ModelTests` round-trips `clockStart`. Update `wiki/architecture.md` "MIDI-in CLOCK".

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
