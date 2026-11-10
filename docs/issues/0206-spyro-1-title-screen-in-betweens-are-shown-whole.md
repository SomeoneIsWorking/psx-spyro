# 0206: Spyro 1 title screen in-betweens are shown whole

Status: open (2026-11-10) for the title's menu and loading modes only; the attract screen (TSM_Init) is
replayed. Owner: `titles/spyro1/core/frame/draw_section.cpp:drawTitle`, `title_slide.*`.

## Observed
Spyro 1, fps60, boot with no input: from the title screen's first frame every in-between was held
(`not-replayed`), so the incoming logo and the PRESS START screen moved at 30 fps. The replay covered only
GamestateDraw's GS_Playing section.

## Cause
GamestateDraw 0x8001ED5C calls TitlescreenDraw 0x8007CEE4 when `g_Gamestate` is 13; that function ends in
the device tail (DrawSync, VSync, PutDispEnv/DrawEnv, DrawOTag), so it cannot be replayed whole, and nothing
else drew the title as a section.

## Fix
`DrawSection::drawTitle` runs the guest's TitlescreenDraw 0x8007CEE4 itself through
`GuestRedraw::dispatchTo`, from its entry up to 0x8007DD0C, the `jal DrawSync` that both the fade's return
(0x8007DD04) and the no-fade branch at 0x8007DCF4 reach; the device tail after it is the real frame's. The
section's `declines` keeps it to TSM_Init; the camera's yaw (+16 per tick) is the camera track.
`TitleSlide` is the logo slides' motion source: the draw places them at `g_TitlescreenAnimIn/Out[m_SubTick]`
(u8[16] at 0x8006FA74/0x8006FA84), so an in-between writes the entry the later tick reads `t` of the way from
the earlier tick's entry. The slide values are whole native pixels, so a half step of an odd step rounds.

## Still held
TitlescreenDraw's menu and loading modes (`m_Mode` 1, 2) are not a replay section; their in-betweens are
held. Demo mode (3) is GamestateDraw's 0x8001E6B8 branch, also not covered.

## Proof
Boot with no input to logic 1016 (logo slide in, `scratch/ram0236/`, `run.py`, `logo.py`, hidden, unpaced,
judgement settings):
- t = 1 (local probe, not shipped): every in-between equals its real frame, full frame, 12 of 12 (`t1`).
- t = 0.5: the logo's lowest edge, reals 119 / 199 / 279 / 367 / 455 / 551 / 655, in-betweens 159 / 239 /
  327 / 407 / 503 / 599 (`ta`, `title/logo5.png`); 596 in-betweens composed, no replay refusal.

Re-proved with the guest's own TitlescreenDraw (2026-11-11, `scratch/review5/`, `attract.py`, `nullcase.py`,
`edges.py`, `border.py`): at t = 1 every in-between of the logo slide in (20), the waiting screen (12) and
logo-out with the border slide (36) is byte-identical to the real present after it; at t = 0.5 the logo's
lowest edge (reals 119 / 199 / 279 / 367 / 455 / 551 / 655 / 759 / 871, in-betweens 159 / 239 / 327 / 407 /
503 / 599 / 711 / 815) and the border's bottom through its bounce sit halfway, and the waiting screen's Spyro
idles smoothly (`half_waiting_spyro.png`). Run-end: 0 intervals refused.
