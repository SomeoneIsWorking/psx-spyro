---
id: 185
title: A solid blue frame flashed between the intro cutscene and the flyby card
status: resolved
symptom: at the end of the intro cutscene's fade-out, one present is full-screen blue (0,0,231) between near-black neighbours
tags: spyro1,render,native,drawenv
created: 2026-10-13
updated: 2026-10-13
---

## Observed
Player build and environment (aspect=1, native render path), `pick spyro1`, the user's pad replayed
(`scratch/s1-play/vrun.py r2 spyro1 24 45 28:600`). Present 363 of the dump is 93% `(0,0,248)`; the
frame log shows it is the first stage-13 frame after stage 14 (GS_Cutscene), drawn with
`isbg=1 bg=(0,0,255)`, the cutscene's `g_BackgroundColor`, and no producer covering it.

## Cause
`FrameRenderer::drawFrame` programmed the DRAWENV (`frameBegin`, which emits the isbg FILL) before
`renderScene`, and the background colours were written inside `renderScene`: `stage13Mode3Render`'s
copy of 0x8001E6B8's clear of `0x80076EF9..FB`/`0x80076F7D..7F`, and the field branch's copy of
0x8001ED5C's `g_BackgroundColor` copy. Retail writes both before its tail's `PutDrawEnv`, so every
stage change cleared with the previous stage's colour for one frame. The attract arm's first frame
draws only text, so the stale cutscene sky showed as a full blue frame.

## Resolution
`game/render/frame/scene_draw_env.{h,cpp}` (`spyro::render::prepareSceneDrawEnv`) owns every
stage's background write and runs before `frameBegin`; the two late writes and the unused
`field_scene_recipe::prepareFrame` are gone. `tests/test_scene_draw_env.cpp`: 3/5 before, 5/5 after.

After: the same window has no blue present and the first stage-13 frame clears `(0,0,0)`; the first
field frame clears with its own sky instead of the previous stage's. Two attract-demo level loads
(no pad, `V_NOPAD=1`) show no solid-colour flash.
