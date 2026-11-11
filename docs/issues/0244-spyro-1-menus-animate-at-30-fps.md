# 0244 — Spyro 1's pause and inventory animate at 30 fps

Status: fixed. Owner: `titles/spyro1/core/frame/draw_section.cpp`.

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

## Fix
`DrawSection` replays the menu draw (`guest::menuDraw`: GS 2 and 3, while `D_800758B8` is set) as a `dispatchTo`
`0x8001A40C` to `0x8001C598`, with PutDrawEnv `0x8005FDD8` passed as a call the real frame already ran
(psxport `GuestRedraw::dispatchTo` stops at each callee entry and steps over the applied ones). The draw still
builds each moby from the whole count; the sprite queue override owns the half step: `actors::DrawnRecords` keeps
each record the last real draw queued, and a replay's record with the same mesh in the same slot is moved `t` of
the way from it (position and angles, the angles' sixteenths passed on). The inventory band is no longer a
producer: its native body widens the band's right edge to the canvas width at its link (`0x8001BEC8`).

## Evidence
`scratch/s1states/go.py menu3` (warp `0/1/0`, Start, Select), judgement settings, unpaced: 0 refusals; the
in-betweens sit between their neighbours (`d1480` 0.28 / 0.40 luma to each side); `menu3-chest.png` shows the
chest turning on every present, `menu3-inv.png` the grid, `band-right.png` the band reaching the right edge of the
684-wide canvas. fps60 on/off fielddigest identical over 4620 fields (`mleak0`, `mleak1`).
