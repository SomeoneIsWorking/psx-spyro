# 0253 — Spyro 1's exit glide: backdrop darkening and fog fill step a tick

Status: open. Owner: `titles/spyro1/core/frame/draw_section.cpp` (`DrawSection::drawExitLevel`) and psxport
`runtime/psx/present/replay_frame.cpp` (in-between assembly).

GS_ExitLevel (10, MenuTransition_Draw `0x8001C694`) replays: Spyro's glide, the camera and the sky's fog progress
move every present (S020). Two of its parts still change only on real presents:

- Pass 0 (`g_nMenuDrawFrames` 0): the frozen backdrop darkens by a LoadImage of its CLUT shifted one entry a tick
  (`0x8001C74C`). A replay cannot upload, and a whole-entry shift has no in-between value, so each in-between shows
  the newest tick's CLUT.
- Pass 1 (`g_nMenuDrawFrames` 1): most of the screen is the back DRAWENV's clear colour, which the draw stores from
  ComputeDepthCuedColor and PutDrawEnv sends. An in-between is the real record with only the section's chain
  spliced in, so it keeps the real PutDrawEnv chain and its fill colour. In `scratch/s020held/u10b` the in-between's
  mean luminance sits 0.85 below the next real frame's against a 5.7 step between real frames.

Proper fix for pass 1: let a section re-link the environment chain it writes (the DRAWENV's packet built from the
blended colour, without the device write), so the in-between's fill comes from the replay. Pass 0 has none short
of drawing the backdrop with a blended palette outside the guest's own path.
