# 0206: Spyro 1 title screen in-betweens are shown whole

Status: closed (2026-11-11). Owner: `titles/spyro1/core/frame/draw_section.cpp:drawTitle`, `title_slide.*`.

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
(0x8007DD04) and the no-fade branch at 0x8007DCF4 reach; the device tail after it is the real frame's. Every
mode but TSM_Demo is replayed: TSM_Init (attract), TSM_Menu (memory-card menu) and TSM_Loading (save
picker) share that draw, which differs only in its sprites. `TitleSlide` is the slides' motion source: the
draw places them at `g_TitlescreenAnimIn/Out[m_SubTick]` (u8[16] at 0x8006FA74/0x8006FA84), so an in-between
writes the entry the later tick reads `t` of the way from the earlier tick's entry; the save picker's borders
slide out through `AnimOut` from its m_State 5. The slide values are whole native pixels, so a half step of an
odd step rounds.

TSM_Demo is GamestateDraw's 0x8001E6B8 branch (the "THE ADVENTURE BEGINS..." card). It waits a single
VSync, so it presents every field: one field per tick, no in-between to replay (`scratch/camfrac/title1`,
card presents all `fields=1/1`). The section declines it.

## Still stepping
The title's fade-outs (TSM_Init sub-state 5, the picker's m_State 5) raise `g_Fade` one a tick and the draw
scales it by 16; a half step cannot be written, so the fade steps at 30 Hz while the border slides at 60.

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

Menu and save picker (2026-11-11, `scratch/camfrac/titleflow.py`, boot to NEW GAME through `drive.Navigator`):
every in-between of the menu (6), the picker (6) and the picker leaving (8) is composed, and at t = 1 each is
byte-identical to its real (`t1title`). At t = 0.5 Spyro idles between his neighbours in the menu
(`menu_spyro.png`) and the leaving border's knob sits halfway (`leave_knob.png`: centres 315 / 265 / 220).
Run-end over the route: 777 intervals replayed, 0 refused, the title slide 4 of 62 items unpaired (each slide's
first tick); fps60 on and off reach identical `fielddigest` over 5709 fields (`tleak-0/1`).
