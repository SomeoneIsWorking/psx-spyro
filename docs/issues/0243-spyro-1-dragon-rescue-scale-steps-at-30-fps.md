# 0243 — Spyro 1's dragon rescue: the shrinking dragon steps at 30 fps

Status: open. Owner: `titles/spyro1/core/frame/draw_section.cpp` (the GS_Dragon step).

## Observed
Warp `0/0/0`, `moby-teleport 250 0` (Artisans' first crystal dragon), judgement settings, hidden, unpaced
(`scratch/s1states/go.py drg3`). Dragon `m_State` 5 (13 steps, about 0.4 s): each in-between equals the tick
before it (luma 0.74/1.04/1.13 to the earlier against 3.86/3.15/2.61 to the later, `between.py`), the figure's
shrink steps (`scratch/s1states/drg-s5-grid.png`). States 0, 1, 4, 6 and 7 move at every present.

## Cause
`0x8001CFDC` state 5 scales `g_SonyImage.u.m_Draw.unk_0x161C`/`unk_0x1622` by `D_8006F3C0[m_CutsceneTicks]`
and `Cos((m_CutsceneTicks - 8) * 42)` between `0x8001F158` and its Memset (`draw.c` GS 8 state 5). The tick is a
whole index read inside the one `dispatchTo` step, so an in-between draws a tick's scale.

## Next
Split the state-5 run at the scale writes and write the two vectors from the table entries `t` of the way
between the ticks.
