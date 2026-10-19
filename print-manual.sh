#!/usr/bin/env bash
set -euo pipefail

# ./print-manual.sh [out.pdf]   docs/MANUAL.md -> PDF (default build/manual/miditin-1-manual.pdf)
#
# pandoc makes one self-contained HTML page (pictures and fonts inlined), headless Chrome prints
# it. Needs pandoc and Chrome/Chromium. Prints the pictures as committed in docs/images; re-render
# them first if the UI changed (MiditinSnap --manual docs/images, wiki/testing.md).
cd "$(dirname "$0")"
out="$(realpath -m "${1:-build/manual/miditin-1-manual.pdf}")"

# $CHROME, else the first that runs: Ubuntu's chromium-browser can be a stub asking for the snap,
# so being on PATH is not enough. Playwright's download is a last resort that is often there.
chrome=""
for c in "${CHROME:-}" google-chrome google-chrome-stable chromium chromium-browser \
         $(ls -d "$HOME"/.cache/ms-playwright/chromium-*/chrome-linux64/chrome 2>/dev/null | sort -rV); do
  if [ -n "$c" ] && "$c" --version >/dev/null 2>&1; then chrome="$c"; break; fi
done
[ -n "$chrome" ] || { echo "print-manual: needs Chrome or Chromium (or set CHROME=/path/to/chrome)" >&2; exit 1; }
command -v pandoc >/dev/null || { echo "print-manual: needs pandoc" >&2; exit 1; }

# Under the project, not /tmp: a snap-packaged Chromium cannot read /tmp.
work="build/manual"
mkdir -p "$work" "$(dirname "$out")"

version="$(sed -n 's/^project(Miditin VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
sha="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "<p class=\"edition\">miditin 1 ${version} &middot; ${sha} &middot; $(date +%Y-%m-%d)</p>" > "$work/edition.html"

# gfm: GitHub's heading ids, so the contents links ("#1-getting-started") land in the PDF too.
pandoc docs/MANUAL.md -f gfm -t html5 --standalone --embed-resources \
  --resource-path=docs --css=docs/manual-print.css \
  --metadata pagetitle="miditin 1 — User Manual" \
  --include-before-body="$work/edition.html" \
  -o "$work/manual.html"

"$chrome" --headless --disable-gpu --no-sandbox --no-pdf-header-footer --print-to-pdf-no-header \
  --print-to-pdf="$out" "file://$(realpath "$work/manual.html")" 2>"$work/chrome.log" \
  || { cat "$work/chrome.log" >&2; exit 1; }
[ -s "$out" ] || { echo "print-manual: $chrome wrote no PDF" >&2; exit 1; }
echo "$out"
