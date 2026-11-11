# 0245 — Spyro 1's respawn and game over are not replayed

Status: open. Owner: `titles/spyro1/core/frame/draw_section.cpp`; the fade's in-between is a presentation-layer
design (psxport `runtime/psx/present/`).

## Observed
GS_Respawn (4) and GS_GameOver (5) draw through `0x8001CA38`, which `spyro1::DrawSection::replays` does not take.
Their in-betweens are composed by the fullscreen tint producer (`game/render/screen_overlays.*`), the last Spyro 1
user of a state producer besides the shaded line (GS 7, flight results, `Flight5`).

## Cause
`0x8001CA38` (`draw.c`) has two phases keyed by `g_GameOverTicks`:
- Ticks 0 to 15, both states: tick 0 MoveImages the last frame into the other buffer and clears `isbg` in both
  draw environments; ticks 1 to 15 each draw only DrawFade(2, 16 then 32) and DrawOTag, so every frame subtracts
  from the picture the frame before left. Tick 15 sets `isbg` again and ClearImages to black; the respawn then
  reloads. A replay of tick k draws its subtraction over a buffer that already holds tick k's, so it would
  subtract twice.
- Game over past tick 15 (`D_80075940 == 1`): a cleared scene, the skybox `0x8004EBA8`, the GAME OVER letters
  built on the HUD moby arena (z from `g_GameOverTextZ[tick]`), PRESS START spun by `COSINE_8(ticks * 4 ..)`, and a
  fade-in tint while ticks < 32. Each phase calls DrawSync, VSync, PutDispEnv, PutDrawEnv and DrawOTag inline.

## What it would take
- The fade (ticks 1 to 15) is a hold the game declares itself: the picture is frozen by MoveImage and `isbg = 0`,
  and it ends in a clear and a reload. Its exact in-between is the last shown present minus `32 * t` per channel
  (subtractive blending clamps at 0, as the guest's does). That belongs to the presenter: an in-between that
  reuses the previous present as its base and adds one subtractive quad of `32 * t`, declared by the title for
  these ticks, with no guest replay. The producer stays until that exists.
- The game-over scene past tick 15 is an ordinary redraw: a section step for `0x8001CA38` from its start to the
  phase's DrawSync, with the letters through `actors::DrawnRecords` (they are built in the buffer's packet pool)
  and `g_GameOverTicks` blended where it indexes `g_GameOverTextZ`.
