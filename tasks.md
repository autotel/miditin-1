# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Requirements filed 2026-10-02 (4 bullets → 4 tasks; 3 landed)

- [opus] **MIDI channel filter.** `Patch::inChannel` (0 = OMNI, 1..16), structural, saved in
  `toXml`/`fromXml`. Engine `handle()`: under a set channel, note on/off, all-notes-off and the
  energy CC from any other channel pass through untouched (not consumed, no held change); MIDI
  clock/start (no channel) unaffected. Output channel: set channel → that channel; OMNI + POLY →
  each voice's notes on its input note's channel (store `ch` beside `held`/`heldVel`, copy into the
  `Chain` on `claimVoice`); OMNI + MONO → channel 1, as now (the bullet keeps channels only for
  poly). Replace `outChannel` with a per-note channel: `Sounding` gains `ch`, so `noteOff` always
  goes out on the channel its note-on used (a retrigger of the same note number from another
  channel's voice offs the old one first -- already the `notes[m].on` path). Held tracking stays
  keyed by note number: the same note on two channels is one held note, first press wins
  (`ponytail:` comment; per-channel keys if it ever matters). Changing `inChannel` in
  `applyPatch` = `releaseHeld` + `releaseAll` (the old channel's note-offs would now be ignored).
  The IN keyboard's `UiEvent::noteOn/noteOff` (`PluginProcessor.cpp:116`) send on the set channel,
  1 under OMNI. UI: a header `RangeRow` "CH" 0..16 beside VOICE, readout `OMNI` / `1`..`16`, built
  like `ccNumber`. Tests (EngineTests): poly OMNI, notes on ch 3 and 5 → outputs and their offs
  on 3 and 5; channel 2 set → ch 1 notes come out verbatim, ch 2 consumed, output on 2; changing
  channel with notes held leaves `sounding() == 0`. Update wiki/architecture.md "Output notes"
  (no longer one channel).

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
