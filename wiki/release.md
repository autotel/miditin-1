# Release build

`.github/workflows/build.yml` builds the release on every push to `main`, on a version tag
(`v0.1.0`, or a bare `0.1.0`) and on a manual `workflow_dispatch`. PRs do not trigger it.
Versioning is by commit: the window shows the build's sha (hover the title), the manual's edition
line prints it, and each run's bundle is the artifact `miditin-1-Linux-<sha>` (a zip, downloadable
from the run's page while GitHub keeps it, 90 days by default). Only a tag also publishes a GitHub
Release. `project(Miditin VERSION ...)` stays as the plugin's nominal version.

- **Linux only.** That is the platform the plugin is checked on. `../Spectral hold plugin`'s
  workflow shows the macOS/Windows matrix rows if they are ever wanted.
- **Siblings.** CMake finds `../JUCE` and `../autotel-plugin-presets-librarian`, so CI checks out
  all three side by side. JUCE is pinned to `8.0.12` (the local checkout is 8.0.12 plus a few
  commits); re-pin both together. The librarian repo is private, so its checkout needs the
  `LIBRARIAN_TOKEN` secret, a fine-grained token with read access to it. Without the secret it
  falls back to the job's own token, which cannot read another private repo.
- **Checks.** Release config, then the test suite and the VST3 host check. HostCheck defaults
  to the RelWithDebInfo path, so CI passes it the Release `.vst3`.
- **Bundle.** `dist/miditin-1/` holds `miditin 1.vst3`, the `miditin 1` Standalone,
  `miditin 1 manual.pdf` and `OFL-IBMPlex.txt`. The binaries embed Plex, and the OFL must ship
  with them. On a tag, the `release` job zips the artifact to `miditin-1-Linux-<sha>.zip` and
  attaches that to a GitHub Release.

## The printed manual

`./print-manual.sh [out.pdf]` (default `build/manual/miditin-1-manual.pdf`): pandoc renders
`docs/MANUAL.md` (read as `gfm`, so GitHub's heading ids and the contents links survive) into one
self-contained HTML page, styled by `docs/manual-print.css`. Headless Chrome then prints it to
A4. It prints `docs/images` as committed and does not re-render them. Local `build.sh` does not
print the manual.

- Chrome is `$CHROME`, or the first candidate that actually runs. On Ubuntu, `chromium-browser`
  can be a stub that only asks you to install the snap. Playwright's cached Chromium
  (`~/.cache/ms-playwright`) is the last resort; that is the one on the dev machine. The
  GitHub runner has `google-chrome`.
- The intermediate HTML goes in `build/manual/`, not `/tmp`, because snap Chromium cannot read
  `/tmp`.
- Chrome subsets the Plex fonts in the PDF. This is allowed: the OFL does not cover a document
  that embeds a font. The "unmodified" rule applies to the font files shipped with the binaries.
