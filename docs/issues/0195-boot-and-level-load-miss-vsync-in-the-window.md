---
id: 195
title: Boot and level load miss vsync in the window
status: open
symptom: the real-window play check's misses, its longest miss run and its worst interval all fall before gameplay
tags: spyro1,spyro2,spyro3,pace,present,playcheck,loading
created: 2026-10-16
updated: 2026-10-16
---

## Observed
Real-window play checks, `warp 10`, 75 s (2026-10-16), after the 0191 and 0192 fixes. Gameplay misses at
most 0.14% of presents and never 2 in a row in all three titles; the rest is before it:

| title | first 1800 intervals missed | longest run | worst |
|---|---|---|---|
| Spyro 1 | 4 | 2 | 60.49 ms |
| Spyro 2 | 42 | 12 | 2005.85 ms |
| Spyro 3 | 64 | 12 | 1755.29 ms |

Spyro 2 and 3 have the run of 12, the multi-second interval and all four beyond-range intervals in the
first 600 presents (boot, before the title); Spyro 3 adds 36 misses in presents 1200 to 1800, around
the level load and the start of gameplay; which of the two is not split yet.

## Gap
Not traced. Next: log present intervals with the frame number through boot and the load, and name the
work inside each long one.
