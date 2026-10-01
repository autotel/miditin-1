# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Phase 0 -- foundation

- [sonnet] Bundle IBM Plex Mono 400/500/600 and IBM Plex Sans Condensed 500/600 via
  `juce_add_binary_data`, plus `Resources/Fonts/OFL-IBMPlex.txt`. Port mepSynth `Source/UI/Fonts`
  (`drawTracked()`) with the prototype's type roles. Palette as named colours in `Source/UI/Theme.h`
  (paper `#fbfaf6`, ground `#e4e0d6`, ink `#1d1c1a`, muted `#8d887e`, orange `#dc6a48`, plus the
  input blue and anything else the prototype's inline styles use -- read them out, do not guess).
- [opus] `wiki/architecture.md`: threading, the audio-thread clock, voice pool, state split,
  how the UI snapshot flows. Add it to the wiki routing table.

## Phase 1 -- state spine

- [opus] `ParamRegistry` (single table, mepSynth pattern) for every continuous control in the
  spec: energy rate / sync division / center / depth / filter time, pulse core params and their
  energy offset/scale, slices, ball count, bounce, base, swing, melody length, transform root /
  octave / oct len, gate. Decide which become host parameters (continuous performance controls,
  yes; structural choices, no -- see CLAUDE.md) and record the why in `wiki/architecture.md`.
- [opus] `Patch` (everything, incl. structural state: typed arpeggio, melody grid with ties,
  pitch-class set, kinds and modes) with `getStateInformation` round-trip; `EngineState` built on
  the message thread, published to the audio thread, outgoing state swapped back (mepSynth
  `EngineState` handoff). Tests: round-trip, publish/swap does not free on the audio side.
- [opus] UI snapshot channel audio → message thread: per-voice energy history point, wheel angle,
  ball positions, digital playhead, generator step, last tone → note mapping, pulse events with
  strength, lit output notes. Lock-free, fixed size, latest-wins; UI polls at its frame rate.

## Phase 2 -- transform (§6)

- [opus] `Transformer`: modes scale (8 scale types, root, whole-octave shift), set (pitch-class
  set, OCT LEN 1-24, fractional octave in semitone steps that also sets the border note), held
  (pool = held notes, tone 0 = lowest). Octave wrap on (continue into next period) / off (fold
  within one period). Exact inverse: input note → pool index k. Pure, allocation-free.
  Tests: every mode × wrap, negative tones, OCT LEN ≠ 12, fractional octave border, inverse ∘
  forward = identity on pool notes, and the held-mode inverse octave bug (§6.5) pinned.

## Phase 3 -- generate (§5)

- [sonnet] Arpeggio: parse typed tones (`0 2 4 7 9`, negatives allowed), orders up / down /
  up-down / random (seeded) / as typed; one step per pulse.
- [sonnet] Input generator: sequence from held input notes, "as typed" = "as played", length =
  held count; in poly a single step ("1", this voice's note).
- [opus] Melody: 16-step grid, length 2-16, tones -7..+7, held notes via ties (one note-on held
  across tied steps, released one gate time after the last step), TRANSPOSE BY IN through the
  transformer inverse (mono: last held note; poly: voice's own note). Spec §9 transpose-centre
  question stays open: implement what the prototype does and note it in the wiki.
- [sonnet] Run modes `free run` / `reset on in`. Tests for every generator and order.

## Phase 4 -- energy (§3)

- [opus] Energy source: osc (sine / triangle / saw / S&H, rate Hz or SYNC 4 bar…1/32 from host
  PPQ with a free-running fallback when the host is stopped, RETRIG), vel (drops to 0 on
  note-off), note (C2..C7 → 0..1), cc (selectable CC number). Center / depth range. Filter off /
  lpf (inertia time constant) / hpf (rate of change over a window) applied to every source. Check
  mepSynth `ModShape.h` for reusable shapes and sync maths before writing new ones. Tests per
  source, sync against a fake playhead, filter step responses.

## Phase 5 -- pulse (§4)

- [opus] Energy modulation per generator: mode scale (p × m) / offset (p + m × range),
  m = offset + scale × energy, offset -1..1, scale -2..2; default scale, 0, ×1. Stored per generator.
- [opus] Wheel: speed with inertia driven by energy as spin force, 3-24 slices, a pulse per peg
  passing 3 o'clock, strength from spin speed, manual spin kick.
- [opus] Shaker: agitation from energy, N balls with ball-ball collisions, restitution; **only wall
  hits** pulse, strength from impact speed; shake kick. Fixed-step, seeded.
- [opus] Digital: base 2/3/5/7, divisions per bar = base × 2ᵏ, energy picks k (always on grid),
  swing for base 2 only, locked to host PPQ when playing.
- [sonnet] None / MIDI in: each incoming note-on is a pulse, strength = velocity.
- Tests: pulse counts and strengths over rendered time for fixed seeds and energies.

## Phase 6 -- MIDI engine (§2, §7)

- [opus] Wire the chain in `processBlock`: control-rate tick, sample-accurate output events,
  strength → velocity, gate length, active-note tracking and guaranteed note-offs (see CLAUDE.md
  invariant). Mono: one chain. Poly: preallocated pool (choose a cap, e.g. 16), a chain per held
  input note created on note-on and released on note-off, tone 0 = that voice's input note located
  in the transformer pool. Input routing per the IN header (pulse · generate · reset · transform).
  Tests on rendered MIDI: no hanging notes under note-on/off storms, mode switches mid-note,
  transport stop, voice-cap overflow.
- [sonnet] Standalone preview: simple internal synth (§7 "Sound") rendering the output notes to
  audio, on by default only in the Standalone; MIDI output device selectable in the Standalone's
  settings. In a DAW the audio output stays silent unless preview is switched on.

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
