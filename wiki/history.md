# History — decisions made and reversed

One entry per reversal: what was tried, why it went, what replaced it. Newest last.

## From the prototype (before any code)

- **Flying tokens / beads along the connection lines** were tried and removed: visually tiring.
  Lines turning orange on transmission replaced them.
- **Shape-based digital pulse display** (line / triangle / square per base) was declined; the grid
  display stays.
- **Held-mode inverse transform** miscounted octaves and transposed several octaves up. Fixed in
  the prototype; the C++ transformer has a test pinning it (spec §6.5).

## Deviations from the v2 prototype (C++ port)

- **Transform wheel labels** in set mode lit the pitch classes *relative to the border note*, so a
  fractional octave lit the wrong labels. The port lights absolute pitch classes; the dots were
  always right.
- **Shaker noise** is scaled by `sqrt(dt / (1/60))`: the prototype added a per-frame random kick
  at ~60 fps, which at the engine's fixed 1 kHz would be four times calmer.
- **No MIDI button / Web MIDI output**: in a plugin the host routes MIDI out; the Standalone's own
  settings pick a MIDI output device. SOUND toggles the preview synth.
- **The CC source's "IN VAL" slider** (the prototype's stand-in for a controller) is gone: the CC
  arrives as MIDI.
- **A melody step whose tone maps outside 0..127** clears the chain's held note instead of letting a
  following tie extend an older one.

## MIDI processor (author, 2026-10-02)

The prototype is a page that plays by itself; the plugin is a MIDI processor -- MIDI in shapes
MIDI out, the IN keyboard is a utility.

- **Silent until input**: no pulse reaches the generator while no input note is held (the physics
  keep moving for the display). Lifting the last key releases a self-running pulse's melody tie;
  under MIDI-in pulse a tie still spans separate key presses, each press being the next step.
- **Non-note input passes through** (CCs, bend, pressure, programs, sysex) at its own sample
  position, merged with the generated notes; input notes are consumed. Bypass still outputs nothing.
- **Defaults are input-driven**: `gen = input`, `tfMode = held` -- the wheel paces an arpeggio of
  the held notes. The factory presets that relied on the old arp/scale defaults now set them.
- **SOUND starts off**, Standalone included (it used to start on there): the preview synth is a
  utility; the product is the MIDI out.
- **IN keyboard muted** (keys at 55 % opacity, title and held list dimmed); OUT keeps the emphasis.
- **Shaker moved by the energy curve** (author): the prototype's per-ball random kicks are gone.
  Balls live in the box's frame -- gravity minus the box's acceleration, the same push for all.
  `STRENGTH`: energy (through OFFSET/SCALE) sets how hard a ~4 Hz hand shakes the box.
  `POSITION`: the box rides the curve vertically (20 ms smoothing), so only changes stir the balls
  -- a slow sine barely, a square edge throws them. Very high agitation pins balls to the walls
  and the pulse rate dips, as a real box shaken hard does. SHAKE ⇄ is one coherent jolt.
  A mode switch re-bases the box (its position stays put, the offset drifts home over ~2 s) and
  the shake amplitude eases in over 20 ms: without that, clicking STRENGTH/POSITION or an S&H
  jump teleported the box and fired a burst of notes.
- **SCALE/OFFSET energy switch removed** (author): it was read as "what energy does", which it
  wasn't, and offset mode only ever added (energy 0..1), so the wheel ran flat out. A brief
  centred-offset fix was replaced by: one mapping, `core * (OFFSET + SCALE * energy)`, plus a
  per-generator ENERGY mode -- wheel SPEED (spin force) / ANGLE (sprung to `SPEED` turns x m, the
  slider reading TURNS; pegs pass only while energy moves; a switch re-bases, no jump), shaker
  STRENGTH / POSITION. Digital has none.
- **Shaker speed cap 4 → 40 /s, hit cooldown 60 → 20 ms** (author: "fast shaker stops detecting
  collisions"). The prototype's 4 /s cap is relative to the box in the box-frame physics, so in a
  fast move the balls rode along with the box and barely met a wall (POSITION, 4 Hz square: 4
  hits in 6 s, now ~590). The display mirrors a ball across the wall it bounced off between two
  snapshots, the trail takes a vertex at each bounce, and the flash ring holds 24.
- **No minimum pulse gap** (author: "let pulses burst"): the prototype's `minPulseGap` (30 ms per
  chain) discarded every hit of a shaker burst after the first, and every other peg of a whipped
  ANGLE wheel. Simultaneous pulses now each advance the generator, at the same sample. The wheel
  counts every peg crossed in a step (a SLICES edit resyncs instead of firing).
- **RUN/STOP and the BPM slider removed; CH / OUT took the header** (author). With no button, a
  saved `run = false` would have been silent for good, so RUN is gone from `Patch` and the engine
  always runs (the host's bypass stops it). `bpm` stays a host parameter -- the tempo with no host
  tempo -- just without a control. OUT exists because mono notes trace back to no one input note,
  so under OMNI there is no input channel to follow.
