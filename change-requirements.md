<!-- New requirements go here as plain bullets. Opus converts them into tasks.md and empties this
     file; the git history of these two files is the record of what was asked and delivered. -->
 - did you follow the design reachable by miditin 1 v2.dc.html? that was the latest design.
- miditin 1 is a MIDI processor: it takes what comes in as MIDI and its main function is sending
  MIDI out. The IN keyboard at the bottom is merely a utility. Concretely (author's picks):
  - input-driven defaults: the default patch reacts to incoming notes out of the box instead of
    self-playing from the wheel
  - pass through non-notes: CCs, pitch bend, aftertouch, program change etc. go out unchanged,
    merged with the generated notes; input notes are still consumed
  - silent until input: no MIDI out at all while no input note is held, whatever the pulse
    generator
  - demote keyboard / synth: the Standalone starts with SOUND OFF; the IN keyboard is visually
    secondary; the host check feeds MIDI in and asserts it shapes MIDI out
