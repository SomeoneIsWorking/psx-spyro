# 0243 — Spyro 1's dragon rescue: the shrinking dragon steps at 30 fps

Status: fixed. Owner: `titles/spyro1/core/frame/draw_section.cpp` (the GS_Dragon step).

## Observed
Warp `0/0/0`, `moby-teleport 250 0` (Artisans' first crystal dragon), judgement settings, hidden, unpaced
(`scratch/s1states/go.py drg3`). Dragon `m_State` 5 (13 steps, about 0.4 s): each in-between equals the tick
before it (luma 0.74/1.04/1.13 to the earlier against 3.86/3.15/2.61 to the later, `between.py`), the figure's
shrink steps (`scratch/s1states/drg-s5-grid.png`). States 0, 1, 4, 6 and 7 move at every present.

## Cause
`0x8001CFDC` state 5 scales `g_SonyImage.u.m_Draw.unk_0x161C`/`unk_0x1622` by `D_8006F3C0[m_CutsceneTicks]`
and `Cos((m_CutsceneTicks - 8) * 42)` between `0x8001F158` and its Memset (`draw.c` GS 8 state 5). The tick is a
whole index read inside the one `dispatchTo` step, so an in-between draws a tick's scale.

## Fix
The state-5 run skips the scale block: `GuestRedraw::dispatchTo` stops at `0x8001D31C` (after `func_8001F158`
returns), `DragonShrink::scale` writes both vectors natively, and the run resumes where the block's branches join
(`0x8001D438`). `DragonShrink`, a motion source, keeps the two ticks' `m_CutsceneTicks`; the first vector is scaled
by the two `D_8006F3C0` entries blended, the second, past tick 8, by Cos of the blended tick's angle
(`AngleTable::atFine`). At t = 1 both are the guest's own values.

## Evidence
`scratch/s1states/go.py drg4` (same route), judgement settings, unpaced: 0 refusals, `dragon shrink: 0 of 12
unpaired`; state 5's in-betweens sit evenly between their neighbours (`d1924` luma 3.02/3.07, 2.68/2.54, 2.36/2.19)
and the wings narrow on every present (`drg-s5-grid-fixed.png`). t = 1 probe (`t1s`): 5 of 5 in-betweens
byte-identical to the next real present. fps60 on/off fielddigest identical over 5676 fields (`sleak0`, `sleak1`).
