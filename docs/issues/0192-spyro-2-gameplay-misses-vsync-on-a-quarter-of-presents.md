---
id: 192
title: Spyro 2 gameplay misses vsync on about a quarter of presents
status: fixed
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

## Cause
`psxport runtime/psx/frame/frame_presenter.cpp:FramePresenter::presentRecords` presented and then paced, so
each present landed on its deadline plus the work before it: the guest step and the in-between's
composition (about 14.6 ms) before the in-between, the t = 1 composition (about 6.4 ms) before the real
present. With MAILBOX the intervals then alternate about 16.7 ms plus and minus the difference, and a
frame whose step grows a few ms crosses 1.5 vsync. All three titles share it.

## Fix
`presentRecords` and `presentCurrent` compose, pace, then present (psxport `docs/presentation.md`, Frame
fence and pacing). `test_record_cadence` (`each_present_follows_its_pace`) and
`test_frame_presenter_contract` pin pace before present; both fail on the old order.

## Measured
Real-window play check, `build/rc5` Clang RelWithDebInfo, shipping settings, `warp 10`, 75 s in Summer
Forest (2026-10-16): `presents: intervals=6105 p50=16.75ms p99=24.25ms worst=2005.85ms
over_1.5_vsync=46 (0.75%) longest_miss_run=12 beyond_range=4`; `records: presents=3435 in_betweens=2671
without_in_between=764 repeats=635 backwards=2`; `audio: reprimes=0 drops=0`. Gameplay (intervals 1800
to 6000): 3 of 4200 missed (0.07%), never 2 in a row, no repeat or backwards step. The whole-run misses,
the run of 12 and the 2 s interval are boot and load (0195); the repeats and steps back are before
gameplay (0187).
