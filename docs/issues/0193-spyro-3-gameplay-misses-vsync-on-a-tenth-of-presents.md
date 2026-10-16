---
id: 193
title: Spyro 3 gameplay misses vsync on about a tenth of presents
status: fixed
symptom: Sunrise Spring after `warp 10` has ~11% of present intervals over 1.5 vsync periods, up to 25% in a window
tags: spyro3,pace,present,playcheck
created: 2026-10-16
updated: 2026-10-16
---

## Observed
Real-window play check (RESTRUCTURE §6), `build/rc5` Clang RelWithDebInfo, shipping settings, `warp 10`,
75 s in Sunrise Spring standing still (2026-10-16). Audio runs at 44100 samples/s throughout.

RunEndReport (whole run): `presents: intervals=6491 p50=16.75ms p99=34.25ms worst=1745.65ms
over_1.5_vsync=645 (9.94%) longest_miss_run=6 beyond_range=4`; `audio: reprimes=0 drops=0`.
Gameplay only (intervals 1800 to 6000): 446 of 4200 missed (10.6%); per 600-interval window 16, 12, 17,
154, 143, 70, 34.

## Cause
The present-then-pace order of 0192 (`FramePresenter::presentRecords`), shared by all three titles.

## Measured
After the 0192 fix, the same check in Sunrise Spring (2026-10-16): `presents: intervals=6489
p50=16.75ms p99=33.25ms worst=1755.29ms over_1.5_vsync=71 (1.09%) longest_miss_run=12 beyond_range=4`;
`records: presents=3651 in_betweens=2839 without_in_between=812 repeats=572 backwards=1`;
`audio: reprimes=0 drops=0`. Gameplay (intervals 1800 to 6000): 6 of 4200 missed (0.14%), never 2 in a
row, no repeat or backwards step. The whole-run misses are boot and the level load (0195).
