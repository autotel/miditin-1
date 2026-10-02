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
