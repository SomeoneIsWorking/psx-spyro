# 0241 — Spyro 1 attract demo fade-out steps at 30 fps

Status: fixed 2026-11-11. Owner: `titles/spyro1/core/frame/screen_fade.*` (`ScreenFade::level`), read by
`DrawSection::drawPlaying`.

## Observed
Spyro 1, idle at the title until the attract demo plays, Cross 40 ticks in (`scratch/fade2/demo.py demo2 40 3 14`,
hidden, unpaced, judgement settings). The fade to black shows each level twice: 99.9th-percentile luma per present
232 232 215 215 199 199 182 182 ..., so every in-between equals the later tick.

## Cause
`titles/spyro1/core/frame/draw_motion.h` tracked `g_Fade` (0x80075918) as a `Countdown`, which takes the later
sample when the word rises, and `DrawSection::drawPlaying` drew `DrawFade(2, g_Fade * 8, ...)` from that whole
word. The demo fade-out raises `g_Fade` by 2 a tick (`g_DemoFadeTimer * 2`, ext `update.c:944-946`), so its
in-betweens drew the later level. The level-entry fade-in falls by `g_DeltaTime` (2) a tick (`update.c:1135-1139`)
and already landed on the odd midpoint. GS_Playing has no other `g_Fade` writer; death, respawn and level exit do not
use it.

## Fix
The `fade` track is gone. `ScreenFade` (formerly the title-only `TitleFade`) samples `g_Fade` with its phase (the
gamestate, plus m_Mode and m_SubState on the title screen) and gives each draw its level `t` of the way between the
two ticks' levels in that draw's units (`* 8` in play, `<< 4` on the title screen). Both draws call it.

## Evidence (`scratch/fade2`)
- Demo fade-out after the fix (`demo3`): 240 232 223 215 207 199 190 182 174 166 157 149 141 133, each in-between at
  its neighbours' midpoint; the reals equal the defect run's (`demo3-sheet.png`).
- Stone Hill entry fade-in (`probe.py entry 0/1/0 400 383 16`): 2 9 17 25 34 42 50 58 67 75 83 91 100, an even step a
  present (`entry-sheet.png`).
- Leak: the demo through the button press and fade-out with fps60 on and off, `fielddigest` identical over 4097
  fields (`leak0.dig`, `leak1.dig`).
