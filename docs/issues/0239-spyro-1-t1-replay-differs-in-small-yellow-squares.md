# 0239 — Spyro 1: a t = 1 gameplay replay differs from its real draw in a few yellow squares

Status: open (2026-11-11). Owner: not yet traced; a gameplay drawer in `DrawSection::drawPlaying`.

## Observed
Warp `0/1/0`, Up+Left held, a local t = 1 probe (`t = 1.0f` in ReplayFrame): 6 of 6 in-betweens differ from
the real present after them only in a few small yellow squares beside a gem's sparkle at the right of the
screen (`psx-spyro` `scratch/camfrac/t1turn.log`, `t1diff.png`; boxes such as (4784, 904)-(5141, 1009) at 8x).
The rest of the frame is identical. The same frames with the camera matrix write disabled are
pixel-identical to these, so the camera track does not cause it. Title-screen replays at t = 1 are exact.

Re-run 2026-11-11 (`scratch/review6/`, `t1turn.log`, `nullcase.txt`, `t1diff.png`), with whole angles on the
guest's own matrices: 6 of 6 differ, all in the same place, about 4480-5130 x 1070-1310. Each differing square
is one of the yellow sparks around Sparx drawn a fraction of a native pixel (2 to 4 output px) off its real
place, same size and colour; nothing else in the frame differs. Spyro 2 and 3 show the same with Sparx's sparks
(0240), so the cause is likely shared.

## Next
Find the drawer of those squares (sparkles `0x80077108` through `glows.cpp`, or particles `0x800573C8`) and
what it reads that the sandbox or a track changes at t = 1 (the plan's Open 10, `g_DeltaTime` ageing inside the
section, is a candidate).
