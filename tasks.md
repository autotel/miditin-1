# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Phase 7 -- UI (§8)

- [opus] Shell: 900 × 600, header (title, chain caption, play/transport, mono/poly), the five
  columns left → right with titles centred above each animation, no boundary lines, IN and OUT
  keyboards along the bottom. Port mepSynth `Tools/RenderSnapshots.cpp` and render the shell
  offscreen to compare against the prototype.
- [sonnet] Energy panel: value-over-time plot in a soft unoutlined circle, clipped on the left only,
  range band, dashed unfiltered line when filtered, source/shape/rate/sync/retrig/filter controls.
- [sonnet] Pulse panel: wheel (pegs, pointer flick, spin button), shaker box (balls, shake
  button), digital one-bar grid (base, swing), none; energy-mod controls. Poly: stacked visuals.
- [sonnet] Generate panel: arp node ring with text entry and order, input steps labelled from 1,
  melody grid (drag-draw, right-edge handle to extend, click to delete, clear, length, TRANSPOSE BY
  IN), run mode. Poly: multiple nodes / playheads.
- [sonnet] Transform panel: 12-tone wheel (C at top), spiral of 3 octaves when wrap on, circle when
  off, click sets root (scale) or toggles notes (set), border note marked, latest mapping flashed
  from the centre; mode, scale, octave, OCT LEN, wrap controls.
- [opus] Connections: energy → pulse moving cable (dash speed and thickness from energy, leaves the
  energy dot at the circle's right edge, enters the pulse centre from the left, drawn over wheel and
  shaker, hidden by the digital grid); pulse → generate slack wire from the wheel pointer, orange
  flash per pulse with thickness from strength; generate → transform dotted line, solid orange
  flash per tone.
- [sonnet] IN keyboard (click latches, vertical click position = velocity, release all, header
  showing what input feeds) and OUT keyboard (lit by velocity, gate control, recent-notes log).

## Phase 8 -- presets and polish

- [sonnet] Preset save/browse via `../autotel-plugin-presets-librarian` (`atl`), as mepSynth
  consumes it; a handful of factory presets, one per pulse generator.
- [opus] Load in REAPER: MIDI out routed to a synth track, sync against the host transport, state
  restore on project reload, no hanging notes on bypass/removal. Record findings in `wiki/testing.md`.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; output velocity design; per-voice transformer in poly;
transform → OUT line.
