# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## Energy additions (filed requirements, 2026-10-02)

Neither is in the spec or the prototype: the filed bullets in `change-requirements.md`'s history
are the source. Record each in `wiki/architecture.md` in a line or two once it lands.

- [sonnet] **One-sided energy filter: BOTH / + ONLY / - ONLY.** New `enum class EnergyEdge :
  uint8_t { both, pos, neg }` in `Patch.h`, field `Patch::enEdge = both` (structural, a choice,
  not a host param), saved by `Document`/`Patch.cpp` like `enFilter` (`edgeNames` "both", "pos",
  "neg"), carried into `EnergySettings::edge` in `Engine::process`. One mechanism serves both
  filters: the smoother `lp` in `Lfo::step` (`Energy.h`) only smooths in the chosen direction and
  **snaps** (`lp = l`) in the other — `pos`: smooth while `l > lp`, snap when `l < lp`; `neg`:
  smooth while `l < lp`, snap when `l > lp`. So LPF `neg` = instant onset, slow release (the
  author's example); HPF `pos` = only rising edges register (`l - lp >= 0`), HPF `neg` = only
  falling. Nothing else in the filter changes. UI (`Shell.cpp`): a second `Segmented` under FILTER,
  `{ "BOTH", "+ ONLY", "- ONLY" }`, shown only when the filter is not off; `Views.cpp` appends
  `+`/`-` to the " lpf"/" hpf" source tag when one-sided. Tests (`EnergyTests.cpp`): LPF `neg` on
  a 0→1 CC step reaches 1 on the next tick and decays slowly on 1→0; LPF `pos` the mirror; HPF `pos`
  never drops below 0.5 on a falling step, HPF `neg` never rises above it. `ModelTests` round-trips
  `enEdge`. A snapshot with LPF + `- ONLY` checked in `MiditinSnap`.

- [opus] **AUDIO energy source: input RMS used directly as the energy level.** Add
  `EnergySource::audio` (last in the enum; `sourceNames` gets "audio"). Signal path:
  - *Processor*: an optional stereo **input** bus (`withInput ("Input", stereo, true)`);
    `isBusesLayoutSupported` accepts input disabled, mono or stereo, output stereo as now. In
    `processBlock` the engine must read the input **before** `buffer.clear()` — move the clear to
    after `engine.process` (the plugin never passes audio through; only the preview synth writes).
  - *Engine*: `process` takes the input channels (`const juce::AudioBuffer<float>&` + the input
    channel count — pass a reference, never copy). Mean square tracked per sample, averaged over
    channels, through a one-pole with a fixed **30 ms** time constant (`// ponytail:` fixed window,
    a knob if it proves too jumpy/slow — the existing INERTIA LPF already smooths further). At each
    tick `EnergyInputs::audio = clamp (sqrt (ms), 0, 1)`, linear, no gain: "directly" per the
    author. One global level, read by every chain (poly voices included). Reset in `prepare`.
    Silence / no input bus = 0.
  - *Energy*: `case EnergySource::audio: l = in.audio;` — center/depth/filter apply as to the other
    sources.
  - *UI*: `enSource` gets "AUDIO" (five options; pick 5 columns or two rows by what fits the
    snapshot); source tag "audio" in `Views.cpp`. No other controls show for it.
  - Tests: engine fed a full-scale-0.5 sine, center 0.5 / depth 1 / filter off, settles to
    ≈ 0.5/√2 (±0.02) within 200 ms and to 0 within 300 ms of silence; same result in mono and
    stereo input. `./build.sh hostcheck` still passes with the new bus (check `enableAllBuses`).
    Note in `wiki/architecture.md` that this source is the one exception to "same MIDI at any
    sample rate": the RMS follows the audio, so it is block-size independent but not bit-identical
    across rates.

## Needs the author

- [author] The manual REAPER pass in `wiki/testing.md` -- needs someone at the screen; agents
  skip it. `./build.sh hostcheck` covers what can be checked headlessly.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; output velocity design; per-voice transformer in poly;
transform → OUT line.
