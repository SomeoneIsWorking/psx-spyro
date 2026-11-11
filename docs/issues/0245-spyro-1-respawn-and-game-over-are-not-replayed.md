# 0245 — Spyro 1's respawn and game over are not replayed

Status: closed, not a defect (2026-11-11). Owner: `titles/spyro1/core/frame/draw_section.cpp`.

## Observed
GS_Respawn (4) and GS_GameOver (5) draw through `0x8001CA38`, which `spyro1::DrawSection::replays` does not take.

## Cause
Each `0x8001CA38` phase waits its own single VSync, so the guest runs these states one logic frame per field.
`presentseq` on the respawn (`scratch/s1states/go.py die2`: warp `0/0/0`, `spyro-at 67082 102277 -8000 1024`)
shows records 2502 to 2516 each `fields=1/1`, the picture dropping about 15 per channel per present, and the game
over (`over1`, the same with `w8 0x8007582C 0` first) 684 frames `fields=1/1`. There is no in-between to draw: the
fade and the game-over scene are already shown at the field rate.
