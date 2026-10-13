---
id: 181
title: The field 2D overlay interpolates across the double-buffer flip
status: resolved
symptom: with fps60 on, every field fade shows half the screen unfaded on each in-between present, alternating top and bottom
tags: spyro1,fps60,hud,fade,overlay
created: 2026-10-13
updated: 2026-10-13
---

## Observed

Player build and environment (`build/player/bin/spyro_port`, no arguments, `tools/shipping_settings.ini`),
picker, `pick spyro1`, the user's pad recording replayed, `preseq 360` 29 s after the pick, covering the
fade-in after the Artisans load. Mean brightness of the top (rows 8-119) and bottom (120-231) halves per
present: 24 presents had a half brighter or darker than both neighbours, e.g. bottom 0.7 -> 91.3 -> 1.3,
top 7.3 -> 133.6 -> 11.7, alternating side every logic frame. All of them were in-between presents.

## Cause

`game/render/hud/field_2d_overlay_recipe.h:derive` stored the fade, border and sprite rects with the
frame's draw offset added. Consecutive frames draw into opposite buffers (y offset 0 and 240), so
`sample` lerped the fade to y 128..352 at t=0.5 and the submitter clipped it to half of the current
buffer. The sprite rects also got the offset twice (`field_collectables` emit adds `gpu.s_off_*`).

## Fix

Overlay rects are offset-free; `Overlay` carries its frame's draw offset and `destination()` adds it for
the fade and border recipes. Sprites keep the guest's own rect. Test: `test_field_2d_overlay_recipe`
`a_buffer_flip_alone_moves_no_draw` (failed before: rect.y 128, want 248). Same measurement after:
1 outlier present in 360, a real present (see below).

## Open

At the end of the fade-out into the load, one real present is solid blue (p0102 in the after run, and
in the before run). Not traced.
