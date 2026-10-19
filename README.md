# miditin 1

A generative MIDI processor (VST3 and Standalone, Linux). Hold notes and it plays new ones. A
slowly moving **energy** drives a **pulse** maker: a spinning wheel, a box of bouncing balls, a
beat grid, or your own playing. Each pulse steps an **arpeggio or melody**, and a **transform**
turns those steps into notes, in a scale or on the notes you hold. Nothing comes out while no
note is held.

![miditin 1](docs/images/overview.png)

**[User manual](docs/MANUAL.md)**: every control, with pictures. The release bundle includes it as a PDF.

## Get it

Every push to `main` builds a bundle on GitHub Actions: `miditin-1-Linux-<sha>` on the run's page
(Actions → Build). Unzip it and put `miditin 1.vst3` in `~/.vst3`, or run the `miditin 1`
standalone. The version is the commit: hover the "miditin 1" title in the window to see which one
is loaded.

## Build

Needs `../JUCE` and `../autotel-plugin-presets-librarian` checked out beside this folder.

```bash
./build.sh            # VST3 + Standalone + tests -> build/Miditin_artefacts/RelWithDebInfo/
./build.sh test       # + run the test suite
./print-manual.sh     # docs/MANUAL.md -> build/manual/miditin-1-manual.pdf (pandoc + Chrome)
```

Development notes are in [CLAUDE.md](CLAUDE.md) and [wiki/](wiki/README.md).

IBM Plex Mono and IBM Plex Sans Condensed are embedded under the SIL Open Font License 1.1
([Resources/Fonts/OFL-IBMPlex.txt](Resources/Fonts/OFL-IBMPlex.txt)).
