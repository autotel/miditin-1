# Release build

`.github/workflows/build.yml` builds the release on every push to `main`, on a version tag
(`v0.1.0`, or a bare `0.1.0`) and on a manual `workflow_dispatch`. PRs do not trigger it.
Versioning is by commit: the window shows the build's sha (hover the title), the manual's edition
line prints it, and each run's bundles are the artifacts `miditin-1-<Linux|macOS|Windows>-<sha>` (zips, downloadable
from the run's page while GitHub keeps it, 90 days by default). Every push to `main` also
publishes a GitHub Release tagged `build-<sha>`, and a version tag publishes one under its own name;
a `workflow_dispatch` on another branch builds without publishing. `project(Miditin VERSION ...)` stays as the plugin's nominal version.

- **Three platforms.** Linux (ubuntu-latest), macOS (macos-latest, universal arm64 + x86_64) and
  Windows (windows-2022, MSVC). Linux is the only one the plugin is developed and checked on:
  the macOS and Windows rows were added blind, so expect first-run compile fixes there. Their VST3
  host check is `continue-on-error` until someone has seen it pass; drop that then.
- **Module config.** JUCE compiles its modules into every target that links them, so the
  librarian's libraries get `JUCE_USE_CURL=0` / `JUCE_WEB_BROWSER=0` too (CMakeLists.txt,
  `MDT_DEFINITIONS`). Without it juce_core wants `curl/curl.h`, which CI runners do not have.
- **Manual.** Printed once by the `manual` job (Ubuntu), then each platform's job stages that PDF.
- **Siblings.** CMake finds `../JUCE` and `../autotel-plugin-presets-librarian`, so CI checks out
  all three side by side. JUCE is pinned to `8.0.12` (the local checkout is 8.0.12 plus a few
  commits); re-pin both together. The librarian is public and checked out at its default
  branch's head, unpinned.
- **Checks.** Release config (`--config Release` for the multi-config MSVC/Xcode builds), then the test suite and the VST3 host check. HostCheck defaults
  to the RelWithDebInfo path, so CI passes it the Release `.vst3`.
- **Bundle.** `dist/miditin-1/` holds `miditin 1.vst3`, the `miditin 1` Standalone (`miditin 1`, `miditin 1.app` or `miditin 1.exe`),
  `miditin 1 manual.pdf` and `OFL-IBMPlex.txt`. The binaries embed Plex, and the OFL must ship
  with them. The `release` job zips each artifact to `miditin-1-<OS>-<sha>.zip` and attaches
  them to the release. It needs every platform green: one failed build, no release.

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
