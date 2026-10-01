# History — decisions made and reversed

One entry per reversal: what was tried, why it went, what replaced it. Newest last.

## From the prototype (before any code)

- **Flying tokens / beads along the connection lines** were tried and removed: visually tiring.
  Lines turning orange on transmission replaced them.
- **Shape-based digital pulse display** (line / triangle / square per base) was declined; the grid
  display stays.
- **Held-mode inverse transform** miscounted octaves and transposed several octaves up. Fixed in
  the prototype; the C++ transformer has a test pinning it (spec §6.5).
