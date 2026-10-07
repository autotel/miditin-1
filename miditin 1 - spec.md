# miditin 1 — spec

A MIDI-generating utility plugin. First in an iteration series exploring generative MIDI; the aim is to eventually merge ideas into one larger plugin.
Prototype: `miditin 1 v2.dc.html` (current), `miditin 1.dc.html` (earlier boxed layout).

---

## 1. Signal chain

Displayed left → right, in processing order:

**ENERGY → PULSE → GENERATE → TRANSFORM → MIDI OUT**

plus an **IN** keyboard (DAW/controller notes) and an **OUT** keyboard along the bottom.

- Generators do not output MIDI notes. They output **relative tones** (integers). The transformer turns tones into real MIDI notes.
- Each pulse carries a **strength** (0..1) that becomes note velocity.

---

## 2. Voice mode (global)

- **MONO** — one instance of energy, pulse and generator for everything.
- **POLY** — each held input note spawns its own full chain (its own energy LFO, its own pulse generator, its own generator). The chain is created on note-on and removed on note-off.
- The **transformer is always mono**: one shared configuration.
- In poly, tone 0 of each voice is that voice's own input note, located inside the transformer pool (see 6.4).
- Poly visuals are stacked: multiple LFO lines, overlapping wheels, shared shaker box with each voice's balls, multiple digital rows lit, multiple arp nodes / melody playheads.

---

## 3. ENERGY (one LFO)

Single LFO. Output: energy 0..1.

**Source**
- `osc` — internal oscillator. Shapes: sine, triangle, saw, sample & hold.
- `vel` — follows incoming note velocity. On note-off the value drops to **0**.
- `note` — follows incoming note pitch (C2..C7 → 0..1).
- `cc` — follows an incoming MIDI CC number (selectable; simulated value slider in the prototype).

**Oscillator options** (osc only)
- **Rate** in Hz, or **SYNC** to DAW tempo with cycle lengths 4 bar … 1/32.
- **RETRIG** — restart the cycle on each incoming note.

**Shaping**
- **Center** and **Depth** define the output range (shown as a band).
- **Filter**, applies to all sources including MIDI-sourced ones:
  - `off`
  - `lpf` — inertia (smooths changes; time constant control)
  - `hpf` — rate of change (window control)

**Display**
- Ordinary value-over-time plot, enclosed in a soft, non-outlined circle.
- The plot line is clipped by the circle only on its old (left) side; it may escape on the right.
- Range band is shown inside the circle. With a filter on, the unfiltered signal shows as a dashed line.

Output velocity is out of scope for now (to be designed later).

---

## 4. PULSE

Each pulse generator has its own core mechanism. Energy modulates that mechanism.

**Energy modulation (per generator, stored separately)**
- Mode: `scale` (parameter × m) or `offset` (parameter + m × range)
- where m = **offset** + **scale** × energy
- Offset range −1..1, scale range −2..2. Default: scale mode, offset 0, scale ×1.

**Generators**
- **Wheel** (wheel of fortune)
  - Core: speed (rev/s). Energy = speed, no inertia (add an LPF to the energy for that).
  - Slices: 3–24. Each peg passing the pointer is a pulse; strength comes from spin speed.
  - Pointer sits at the right (3 o'clock), flicks downward with the pegs' motion. A spin button gives a manual kick.
- **Shaker**
  - Core: agitation. Energy = agitation.
  - Balls bounce with simple physics (ball–ball collisions included). **Only wall collisions** are pulses; impact speed sets strength.
  - Settings: ball count, bounce (restitution), shake button.
- **Digital**
  - Core: grid division. Energy = rate, always quantized to the grid.
  - **Base** sets the grid family, using the first primes 2, 3, 5, 7. Divisions per bar are base × 2ᵏ, e.g. base 2 → 1/2…1/32, base 3 → 1/3…1/48. The display shows one bar.
  - Swing (base 2 only).
  - A shape-based rendering (line / triangle / square per base) was discussed and **declined**; the grid display is kept.
- **None / MIDI in** — no generator. Each incoming MIDI note is a pulse.

---

## 5. GENERATE

Outputs relative tones (integers), advancing one step per pulse.

- **Arpeggio** — user-typed tones (e.g. `0 2 4 7 9`). Order: up, down, up/down, random, as typed. Shown as a ring of nodes.
- **Input** — the arpeggio is built from the held input notes. Length = number of held notes. Order "as typed" becomes "as played".
  - In **poly**, input mode is a single step, labeled **1** ("this note").
  - Input steps are labeled from 1.
- **Melody** — a 16-step grid (length 2–16), tones −7..+7.
  - Draw lines by dragging; each step becomes a one-step note.
  - **Held notes**: a note gets longer only by dragging its right-edge handle. Drawing never creates long notes.
  - A held note sends one note-on, holds through its tied steps, and releases one gate time after its last step.
  - Clicking a note removes it. Clear button.
  - **TRANSPOSE BY IN** (toggle): shifts the melody by the input note, measured by running the transformer backwards on it, so tone 0 lands on the input note. Mono uses the last held note; poly uses each voice's own note.

**Run mode**
- `free run`, or `reset on in` (generator restarts at each incoming note).

---

## 6. TRANSFORM

Maps relative tones to MIDI notes. Always mono (one configuration).

### 6.1 Modes
- **Scale** — root + scale type (major, minor, dorian, phrygian, pentatonic major/minor, whole tone, chromatic) + octave (whole octaves).
- **Set** — a custom set of pitch classes.
- **Held** — the notes currently held in the DAW. Tone 0 is the lowest held note, tone 1 the next, and so on.

### 6.2 Display / interaction
- A **12-tone wheel** with C at the top.
- With octave wrap on, it is drawn as a **spiral** (3 octaves). With wrap off, it is a plain **circle**.
- Set mode: notes are turned on and off by clicking the wheel itself. Scale mode: clicking sets the root.
- The latest tone → note mapping flashes as a line from the centre.

### 6.3 Set-mode extras
- **Octave length** (OCT LEN, 1–24 semitones): the repeat interval of the set. This allows scales that do not repeat at the octave, e.g. one note every 5 semitones.
- **Octave** slider accepts fractions in set mode, in semitone steps. It sets both the transposition and the **border note** where the wraparound happens (marked on the spiral).

### 6.4 Octave wrap
- **On** — tones beyond the pool continue into the next octave (or next OCT LEN period).
- **Off** — tones fold back within one period.

### 6.5 Inverse transform
- Used in poly and by TRANSPOSE BY IN: find the pool index k of an input note, then apply tone + k.
- Fixed bug: in held mode the inverse miscounted octaves and transposed several octaves up.

---

## 7. IN / OUT

- **IN** keyboard: click to latch notes. Click position on a key sets velocity (lower = louder). A real MIDI controller works via Web MIDI. Release-all button. The header shows what the input currently feeds (pulse · generate · reset · transform).
- **OUT** keyboard: lit notes, brightness from velocity. Gate (note length) setting. Recent-notes log.
- **Sound**: simple internal synth for previewing.
- **MIDI**: Web MIDI output to the DAW, with an output selector.

---

## 8. Visual language

- Schematic and animated. The process is readable left → right.
- **No boundary lines** between modules. Titles are centred above each animation, with no numbers. Minimal explanatory text (reduced on request).
- Palette: warm paper ground, near-black ink, one orange accent, one blue for input. Type: IBM Plex Mono with IBM Plex Sans Condensed for the title.
- **Connections** are drawn as lines that show how things link:
  - **Energy → pulse**: a moving cable. It leaves the energy dot at the right edge of the circle and enters the **centre** of the pulse animation from the left (end handle points left). Dashes flow faster and the line thickens with energy. It is drawn over the wheel and shaker, but the digital grid hides it.
  - **Pulse → generate**: a slack wire from the pulse output (the wheel pointer) to the generator centre. It flashes orange on each pulse, thicker for stronger pulses.
  - **Generate → transform**: a dotted line to the wheel centre. It flashes solid orange on each tone.
- Lines turn orange on transmission. Flying tokens and beads were tried and **removed** because they were visually tiring.
- Plugin window: 900 × 600.

---

## 9. Open questions / later

- **Transpose centre** for TRANSPOSE BY IN. Options raised:
  1. a centre-note setting (no transpose at the centre)
  2. anchor to the first note of a phrase
  3. fold the shift into ±½ octave
- **Output velocity** design (currently: pulse strength → velocity).
- Whether the transformer should ever be per-voice in poly.
- Line from transform → OUT (not drawn yet).
