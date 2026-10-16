---
id: 191
title: Spyro 1 gameplay runs at about a third of real time in the window
status: fixed
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

## Cause
`game/render/field/guest_working_memory.cpp:HostMemory::mapEmpty` mapped through `map`, which seeds every
byte from the guest (an overlay hash lookup and `Core::mem_r8` per byte) and then zeroed the range. Every
producer render of an in-between maps the 112 KiB packet arena, so each present seeded megabytes byte by
byte: 75% of host time in a headless profile, 11.4 real frames/s unpaced. The invalidation count is not
the cost: 43 blocks are revoked in a run; the calls are native packet stores through `Core::mem_w32` that
find no translated code.

The vsync misses that remained (about 11% in gameplay) were the shared pacing order of 0192/0193.

## Fix
`HostMemory::place` merges a mapping and takes its seed: `map` seeds from the guest, `mapEmpty` starts
zeroed and never reads it. `test_guest_terrain_memory` (`testEmptyMappingIsAFill`) maps the 2 MiB window
empty 64 times inside 40 ms: about 400 ms before, about 3 ms after. Unpaced gameplay went to 40.8 real
frames/s.

## Measured
Real-window play check, `build/rc5` Clang RelWithDebInfo, shipping settings, `warp 10`, 75 s in Artisans
(2026-10-16), with the 0192 fix: `presents: intervals=6911 p50=16.75ms p99=19.75ms worst=77.31ms
over_1.5_vsync=9 (0.13%) longest_miss_run=2 beyond_range=0`; `records: presents=3676 in_betweens=3236
without_in_between=440 repeats=0 backwards=1`; `audio: reprimes=0 drops=0`, 44080-44132 samples/s.
Gameplay (intervals 1800 to 6000): 5 of 4200 missed, never 2 in a row. The run of 2 is in the first 600
presents (boot, 0195); the backwards step is 0194.
