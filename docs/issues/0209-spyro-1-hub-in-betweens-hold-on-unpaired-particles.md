# 0209 — Spyro 1 hub in-betweens hold on unpaired particles

Status: closed (2026-10-27). Owner: psxport `runtime/psx/present/frame_composer.cpp` (identity).

## Observed
Spyro 1 `warp 10` (hub), unpaced at ires 0, fps60, 3840x2160: the owner journal's top rows were
`IntervalHeld/unpaired` for producer `0x800573C8` (1121 intervals lost) and `0x800584C4`, and the hub presented at
the logic rate.

## Cause
`FrameComposer` refused an interval whenever S held an object S' did not, so every newly spawned particle held it.

## Fix
An object born in S (no state in S', no rest) is absent from the in-betweens and appears at `t = 1`; only an object
S' holds ambiguously is `unpaired`. Shown in `scratch/redesign-review/s1hub`: no hub rows, every hub interval
composed at `t = 0.5, 1`.

## Remaining
A newborn particle could move from its spawn state instead of popping in at `t = 1`; that needs spawn rests
(stage 4 incarnations).
