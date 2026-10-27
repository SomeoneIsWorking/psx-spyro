# 0209 — Spyro 1 hub in-betweens hold on unpaired particles

Status: open. Owner: the particle producers `0x800573C8` and `0x800584C4` (`game/render/`).

## Observed
Spyro 1 `warp 10` (hub), unpaced at ires 0, fps60, 3840x2160 (2026-10-27): the owner journal's top rows are
`IntervalHeld/unpaired` for producer `0x800573C8` (1121 intervals lost) and `0x800584C4`. The hub presents at the
logic rate: each held in-between is pixel-identical to its `t = 1` present.

## Cause
Since one mode per interval (psxport `docs/presentation.md`), an object with state in S and none in S' holds the
interval instead of drawing S's moment at `t = 1`. Particles are spawned every frame, so almost every hub interval
has a new one.

## Proper fix
Spawn identity (stage 4 incarnations): a particle born in S rests at its spawn state, so it moves from there.
