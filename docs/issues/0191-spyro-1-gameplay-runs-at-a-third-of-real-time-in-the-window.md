---
id: 191
title: Spyro 1 gameplay runs at about a third of real time in the window
status: open
symptom: Artisans after `warp 10` produces ~16000 audio samples/s of 44100 and misses vsync on almost every present
tags: spyro1,pace,present,playcheck
created: 2026-10-16
updated: 2026-10-16
---

## Observed
Real-window play check (RESTRUCTURE §6), `build/rc5` Clang RelWithDebInfo, shipping settings, `warp 10`,
75 s in Artisans standing still (2026-10-16). Audio production is 44100 samples/s on the title and falls
to 15000–17500 (6500–8700 for ~12 s) once the level runs; 772 guest frames in 75 s.

RunEndReport (whole run): `presents: intervals=3955 p50=20.50ms p99=105.25ms worst=166.76ms
over_1.5_vsync=1566 (39.60%) longest_miss_run=1552 beyond_range=12`; `audio: reprimes=0 drops=0`.
Between presents 2400 and 3600, 1198 of 1200 intervals miss vsync.

Not the warp: the same window with no command reaches the attract demo through the title's own
TSM_Demo handoff, and its level also drops to 19000–31000 samples/s (`over_1.5_vsync=505 (9.89%)`,
`longest_miss_run=97` over a run that is mostly title).

The run end reports `invalidations=67778858` over 2198 steps, and issue 0190's line fires ~1000 times a
second during the level.

## Gap
Not traced. Next: profile one level frame and see whether translated-block invalidation or the terrain
in-between (0190) owns the time.
