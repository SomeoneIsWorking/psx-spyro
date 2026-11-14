# 0253 — Spyro 1's exit glide: backdrop darkening steps a tick

Status: open (pass 0 only). Owner: `titles/spyro1/core/frame/draw_section.cpp` (`DrawSection::drawExitLevel`) and
psxport `runtime/psx/present/replay_frame.cpp` (in-between assembly).

GS_ExitLevel (10, MenuTransition_Draw `0x8001C694`) replays: Spyro's glide, the camera and the sky's fog progress
move every present (S020). Reached by `warp 0/1/2` (Stone Hill's exit arrival); the glide runs from about step 1429
to 1491 of that run.

- Pass 0 (`g_nMenuDrawFrames` 0): the frozen backdrop darkens by a LoadImage of its CLUT shifted one entry a tick
  (`0x8001C74C`). A replay cannot upload, and a whole-entry shift has no in-between value, so each in-between shows
  the newest tick's CLUT. Nothing short of drawing the backdrop with a blended palette outside the guest's own path
  would give it one; left as is.
- Pass 1 (`g_nMenuDrawFrames` 1), fixed: most of the screen is the back DRAWENV's clear fill, stored from
  `ComputeDepthCuedColor(g_dwWorldFogColor, 0, progress)` and sent by PutDrawEnv. The replay stops at PutDrawEnv,
  stores the blended progress's colour into the DRAWENV, builds its DR_ENV with SetDrawEnv (no device write) and
  relinks it (`ReplayFrame::relink`), so the in-between's fill replaces the real frame's. The fill colour is halfway
  in 24 bits (ticks 0xA/0xB: `1F 29 4C` and `23 2E 53`, in-between `21 2B 50`); the fill lands in 15-bit VRAM as on
  the GPU, so each channel shows its 5-bit truncation. The real colour climbs about one 5-bit level a tick, so an
  in-between matches one neighbour per channel and the screen's luminance steps at in-betweens as well as at real
  presents (`scratch/s0253/exit_glide_fill_crops.png`). Logic RAM at step 1485 is identical with fps60 on and off.
