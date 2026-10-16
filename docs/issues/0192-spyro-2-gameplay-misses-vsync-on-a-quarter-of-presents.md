---
id: 192
title: Spyro 2 gameplay misses vsync on about a quarter of presents
status: open
symptom: Summer Forest after `warp 10` has ~27% of present intervals over 1.5 vsync periods at full audio rate
tags: spyro2,pace,present,playcheck
created: 2026-10-16
updated: 2026-10-16
---

## Observed
Real-window play check (RESTRUCTURE §6), `build/rc5` Clang RelWithDebInfo, shipping settings, `warp 10`,
75 s in Summer Forest standing still (2026-10-16). Audio runs at 44100 samples/s throughout.

RunEndReport (whole run): `presents: intervals=6103 p50=16.75ms p99=35.50ms worst=2004.92ms
over_1.5_vsync=1500 (24.58%) longest_miss_run=12 beyond_range=4`; `audio: reprimes=0 drops=0`.
Gameplay only (intervals 1800 to 6000): 1124 of 4200 missed (26.8%). The 2 s worst interval and the
four beyond-range intervals fall in the first 600 presents, before the title.

## Gap
Not traced. Next: log the interval sequence in gameplay and see whether the misses are periodic (a
field cadence) or follow guest work.
