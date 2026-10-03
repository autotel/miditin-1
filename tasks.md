# Tasks

Open work only. Each entry carries the model that should implement it -- `[opus]` for the
delicate, the architectural or the not-obviously-right; `[sonnet]` for the mechanical or the
fully specified. Delete an entry as it lands. Durable knowledge goes in `wiki/`, never here.

Source for every phase: `miditin 1 - spec.md` (§ numbers below) and the v2 prototype
`Miditin 1 plugin design/miditin 1 v2.dc.html`, whose script is the reference maths for each stage.
Port behaviour from the prototype; do not reinvent it. Each phase ends with `./build.sh test` green
and one commit per task.

## CC LEARN

### 1. [sonnet] LEARN: pick the CC from the last few that arrived
Bullet (acf86b0, amended in the next filing): a learn button in CC mode that lists the last few
*different* CCs that arrived, to pick from -- so one busy CC cannot block learning another.
- Engine: a move-to-front list of the last 4 distinct CC numbers consumed (`ours` in
  `Engine::handle`, any number, not just `ccNumber`; 0..119 only -- 120..127 are channel-mode
  messages and `ccNumber` stops at 119). Fixed array, audio thread. Published in `UiSnapshot`
  (`recentCc`, `recentCcCount`), newest first.
- The audio thread never writes `Patch`; picking is the editor's `edit`.
- UI: `BoxButton` "LEARN" beside the `CC #` row (shown under `EnergySource::cc` only); click
  toggles learning (editor state, not `Patch`). While learning, a row of up to 4 `BoxButton`
  chips under it shows `recentCc` live ("CC 74"), the current `ccNumber` lit; clicking one sets
  `ccNumber` and stops learning. Empty list: one muted "move a control" line instead.
- Test: CCs 1, 74, 1, 11 → list 11 1 74; 120+ and other channels ignored; a fifth distinct
  number drops the oldest. Snapshot scene under CC with learning on.

## Open (spec §9) -- not tasks until filed as requirements

Transpose centre for TRANSPOSE BY IN; per-voice transformer in poly;
transform → OUT line.
