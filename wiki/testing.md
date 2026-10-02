# Testing

| Command | What it proves |
|---|---|
| `./build.sh test` | the headless suite: transformer, generators, energy, pulse physics, rendered MIDI (determinism across block size and sample rate, no hanging notes under storms / mode switches / voice-cap overflow / transport stop), state documents, the librarian's own tests |
| `./build.sh sanitize` | the same under ASan/UBSan |
| `./build.sh hostcheck` | loads the built **VST3** through JUCE's VST3 hosting, no window: MIDI in shapes MIDI out (silent with nothing held, the held chord arpeggiated, a CC passed through; the wrapper's `MIDI CC` params are skipped in the state compare), transport, stop releases, state reload (every parameter, then byte-exact), bypass releases |
| `build/MiditinSnap_artefacts/RelWithDebInfo/MiditinSnap <dir>` | renders the editor to PNGs from a headless engine run (several scenes + the preset panel); compare with the v2 prototype |
| `xvfb-run -a timeout 8 ./run-built.sh` | the Standalone starts and runs (exit 124 = still alive at the timeout) |

Never open a window on the user's display (CLAUDE.md); everything above runs headless.

## Why a host check instead of driving REAPER

REAPER can render a project from the command line, but that runs with the user's REAPER config
(plugin cache, recent files). `MiditinHostCheck` exercises the same wrapper code paths a DAW does
without touching it.

## Manual pass in REAPER (needs the author)

Point REAPER at `build/Miditin_artefacts/RelWithDebInfo/VST3/` (not installed to `~/.vst3`).

1. Track 1: miditin 1. Track 2: any synth, receiving MIDI from track 1 (send, MIDI only). Arm
   track 1 with a MIDI controller as input.
2. Play: notes reach the synth; the digital pulse lands on the grid against the host bar; energy
   SYNC follows the tempo.
3. Stop mid-note, bypass mid-note, delete the FX mid-note: nothing hangs on the synth.
4. Save, close, reopen the project: every control and the open preset's name come back.
