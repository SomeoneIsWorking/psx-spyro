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
`DrawSection::drawTitle` replays TitlescreenDraw's TSM_Init body (spyro-1 decomp
`src/overlays/titlescreen.c`): the substate's TitlescreenDrawSprite 0x8007CD38 calls, 0x800521C0,
0x8001F158, the two Memsets of 0x8006FCF4, 0x8001F798, the 0x800785D0 store, 0x800258F0, 0x8004EBA8 and the
fade 0x800190D4. `replays()` runs it only in TSM_Init; the camera's yaw (+16 per tick) is the camera track.
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
