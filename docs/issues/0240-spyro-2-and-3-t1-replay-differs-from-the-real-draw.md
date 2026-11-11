# 0240 — Spyro 2 and 3: a t = 1 gameplay replay differs from its real draw

Status: open (2026-11-11). Owner: not yet traced; a drawer in the Spyro 2/3 draw section (`game/core/draw_section.*`).

## Observed
Warp `0/1/0`, Up+Left held, judgement settings, hidden, unpaced, a local t = 1 build (a copy of the tree with
`t = 1.0f` in `ReplayFrame::beforeDraw`, `psx-spyro` `scratch/review6/hc`, not shipped): every in-between differs from
the real present after it (`scratch/review6/nullcase.txt`).
- Spyro 2 (`t1-s2diff.png`): the yellow sparks around Sparx are drawn a fraction of a native pixel off their real
  place, as Spyro 1's 0239; one small spot on a far moby.
- Spyro 3 (`t1-s3diff.png`, `t1-s3spot.png`): Sparx's sparks the same way, and at about (3800, 650) at 8x a
  peach terrain sliver behind a dark wall's edge is cut differently: the replay shows a narrower strip.
- The same boxes with `PSXPORT_DEPTH=0` (`t1-s3spot-nodepth.png`), so native depth does not cause them.

## Next
Find what the sparks' drawer reads that differs at t = 1 (shared with 0239), and which Spyro 3 terrain or wall draw
emits the sliver and what it reads that the replay changes.
