# MIDITIN 1 — coding agent entry point

JUCE 8 VST3/Standalone **generative MIDI plugin** (Linux). A chain read left → right:
**ENERGY** (one LFO, or velocity / pitch / CC of the input) drives a **PULSE** generator (wheel of
fortune, ball shaker, digital grid, or the MIDI input itself); each pulse advances a **GENERATE**
stage (arpeggio, held-input arp, 16-step melody) that emits *relative tones*; **TRANSFORM** maps
tones to MIDI notes (scale, custom pitch-class set, or the held notes). Mono, or poly where every
held input note spawns its own energy/pulse/generate chain. First in a series of generative-MIDI
experiments meant to merge into one larger plugin later.

Two specs, both authoritative, both kept verbatim:
- `miditin 1 - spec.md` — the product and its behaviour, section by section.
- `Miditin 1 plugin design/miditin 1 v2.dc.html` — the **current UI prototype**, a working
  browser model of the whole plugin: layout, palette, type, animation, interaction, and the
  reference implementation of the generators' maths. `miditin 1.dc.html` is the earlier boxed
  layout, kept for history only. When the spec and the v2 prototype disagree, ask; unattended, follow
  the spec and note the deviation in `wiki/history.md`.

For anything past this file's depth, check the **[wiki](wiki/README.md)** routing table before
searching from scratch.

## Build

```bash
./build.sh          # configure + build (VST3 + Standalone + tests)
./build.sh test     # + run the headless suite
./build.sh sanitize # the suite under ASan/UBSan (build-asan/)
./build.sh hostcheck # load the built VST3 headlessly: MIDI out, state, bypass
```
UI checks: `MiditinSnap <dir>` renders PNGs offscreen — see [wiki/testing.md](wiki/testing.md).
RelWithDebInfo deliberately — a Debug VST3 in a DAW is far slower and has `jassert` armed (a fired
jassert is a SIGTRAP that takes the host down). `cmake --preset debug` → `build-debug/`, for gdb
only. Standalone: `./run-built.sh`. VST3: `build/Miditin_artefacts/RelWithDebInfo/VST3/miditin 1.vst3`
— NOT installed to `~/.vst3`; point the DAW at this build output. **Rebuilding while the host is
open changes nothing until the plugin is reloaded.** Crash in REAPER: `./debug-reaper.sh`.

## Invariants

- **The audio thread owns time.** The whole chain — energy, pulse physics, generator, transform,
  gate scheduling — runs inside `processBlock`, at a fixed control rate, emitting MIDI with sample
  offsets. The UI never decides when a note happens; it animates from a snapshot the audio thread
  publishes. A UI that is closed, slow or absent produces exactly the same MIDI.
- **Simulations are deterministic.** The wheel and shaker are physics, but seeded and stepped at a
  fixed rate, so a test can render N seconds of MIDI and assert on it. No `juce::Random` default
  seeds, no wall-clock reads, no frame-rate dependence.
- **Every note-on has a note-off.** Track active output notes; release them on gate end, voice
  removal, mode/structure change, bypass, transport stop and `releaseResources`. A hanging note is
  the worst bug this plugin can have.
- **The transformer is a pure function** of (tone, config, held notes) → note, with an exact inverse.
  It is always mono — one configuration shared by every voice.
- **Poly voices are preallocated.** Note-on claims a chain from a fixed pool; nothing is allocated,
  locked or logged on the audio thread. `juce::AudioBuffer` / `MidiBuffer` assignment copies — pass
  references.
- **One table holds every range, default and unit** (`Source/Model/ParamRegistry.cpp`). Host
  automation, readouts and saved state all derive from it. No compatibility shims: there are no
  users but the author, and clean code beats an old patch loading.
- **Structural state is not automatable**: anything that reshapes the chain (pulse generator kind,
  generator kind, transform mode, voice mode, a typed arpeggio, the melody grid, the pitch-class set).

## Neighbouring projects — read before reinventing

- `../mepSynth-plugin` — the sibling this project's setup is copied from. `Source/Model/ParamRegistry`
  (the single-table pattern), `Source/DSP/EngineState.h` + `PluginProcessor.cpp` (message-thread
  builds state, audio thread reads it, outgoing state swapped back), `Source/DSP/ModShape.h` (LFO
  shapes, tempo sync), `Source/UI/Fonts.{h,cpp}` (`drawTracked()` — `juce::Font` has no
  letter-spacing), `Tools/RenderSnapshots.cpp` (offscreen render to check UI against the design),
  `Tests/main.cpp` (the headless runner), `CMakeLists.txt` (core-sources split for the tests).
- `../Sampler-plugin` — origin of `ModShape`, `HelpBar` and the `EngineState` handoff.
- `../autotel-plugin-presets-librarian` — the shared preset librarian (`atl`), consumed with
  `add_subdirectory`.

## Important

- Use `<juce_module/juce_module.h>` includes, NOT `<JuceHeader.h>` (CMake build, not Projucer)
- JUCE path: `../JUCE`
- Everything non-UI goes in the core-sources list in CMakeLists.txt so the tests can link it
- IBM Plex Mono / IBM Plex Sans Condensed are SIL OFL 1.1 with Reserved Font Name "Plex": embed
  them **unmodified** (no subsetting into a renamed font); the OFL text ships with any binary
- Put durable knowledge in `wiki/`, not here or in `tasks.md` — never the same fact twice
- **Never open a window on the user's display.** Check the UI with the offscreen snapshot tool
  (`MiditinSnap`, no window at all). If the running Standalone is truly needed, run it
  under `xvfb-run -a` — never on `DISPLAY=:0` / Wayland.

## Unattended runs

`./continue-work.sh` runs this pipeline headlessly: `claude -p "continue the work"` in auto mode,
one run at a time (flock), logging to `~/.local/state/miditin-continue/` with `latest.log`
symlinked to the newest. It sets its own PATH, because `at` and systemd hand a job a near-empty
environment. Schedule one with `echo '"/home/joaquin/dev/miditin 1/continue-work.sh"' | at 00:02`
(the path has a space); `atq` lists what is queued and `atrm <job>` cancels it.

## Product development input pipeline

Defined by `working-logic.md`:
- New requirements go in `change-requirements.md`. **Commit that file on its own, as filed, before
  converting anything.** Opus then turns the bullets into detailed tasks in `tasks.md`, each
  tagged with the model that should implement it, and empties the requirements file. Implementers
  work from `tasks.md` and delete entries as they land.
- If `tasks.md` has open work: implement it step by step.
- If `tasks.md` is empty but `change-requirements.md` is not: Opus writes the plan first.
- If both are empty: hunt bugs, or make the code/docs/agent setup leaner.
