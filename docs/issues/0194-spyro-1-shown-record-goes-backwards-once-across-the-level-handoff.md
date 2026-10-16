---
id: 194
title: Spyro 1 shown record goes backwards once across the title-to-level handoff
status: open
symptom: `records: backwards=1` in the 600 presents containing `warp 10` and the Artisans load
tags: spyro1,present,playcheck
created: 2026-10-16
updated: 2026-10-16
---

## Observed
Real-window play check, `warp 10` (2026-10-16), two runs: `records: presents=2198 in_betweens=1758
without_in_between=440 repeats=0 backwards=1`; the periodic counter puts the step between presents 600
and 1200, which hold the warp (f601), the flyby and the level load. The same window with no command
through the attract demo's TSM_Demo handoff shows `backwards=0`.

The warp's one difference from the demo handoff is the reset call (0x8001277C) that New Game runs
before it; whether New Game shows the same step is not measured.

## Gap
Not traced. Next: count the backwards present with its frame number and compare the displayed buffer
before and after the reset.
