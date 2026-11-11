# 0244 — Spyro 1's pause and inventory animate at 30 fps

Status: open. Owner: `titles/spyro1/core/frame/draw_section.cpp`.

## Observed
Warp `0/1/0`, Start (pause) then Select (inventory), judgement settings, hidden, unpaced, with a trial section
for `0x8001A40C` (`scratch/s1states/go.py menu2`; not kept): 0 refusals, yet every in-between equals the tick
before it (luma 0.01 to the earlier against 0.4-0.6 to the later, `menu-d1340.png`, `menu-d1480.png`). Without
the section the in-betweens are composed by the inventory band and shaded line producers.

## Cause
The menu draw reads its own draw count `D_800758B8` whole for every turning moby (`COSINE_8(D_800758B8 * 8 + i *
12)`) and the cursor (`func_80018534(.., D_800758B8 * 2, ..)`); no whole count names a half step, and the 256-entry
table index moves by 8 a count. It also opens with PutDrawEnv (`0x8001A5FC`), a device call a replay must step
over (a stand-in that returns at once inside a replay was enough).

## Next
The section for `0x8001A40C` with those rotations written from the count `t` of the way on (a native body for the
rotation writes, or a split at each), then the band and line producers go for the menus.
