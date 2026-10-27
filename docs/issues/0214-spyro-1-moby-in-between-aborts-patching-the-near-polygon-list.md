# 0214 — Spyro 1 aborts in a moby in-between that patches the near polygon list

Status: open (2026-10-28). Owner: `titles/spyro1/render/actor_emitter_producer.cpp` (`ActorEmitterProducer::render`).

## Observed
`tools/oracle_compare.py --route <walk.pad> --frame-step 1` (fps60 from `tools/shipping_settings.ini`, hidden window)
aborted the product at REPL frame 7513, screen key 0x37:
`[emit-memory:error] a render wrote 4 bytes to 0x800728F4, outside its host ranges`, from
`ActorEmitter::patchNear` <- `ActorEmitter::draw` <- `ActorEmitterProducer::render` <- `FrameComposer::place`.
A player at fps60 hits the same abort wherever this moby is drawn near the camera.

## Suspected cause (not yet traced)
The in-between maps the near polygon list as `[nearPolygons, nearPolygons + word0 + 4)`, but `patchNear`
(0x8001F910) writes each run to `(run & 0xFFFF) + nearPolygons` for `run >> 24` words, so a run whose target lies past
that extent writes unmapped memory. The mapped extent should be the span the frame's patch runs write.

## Reproduction
`scratch/player-bugs-3/vib/walk_taps.pad` with the command above; the native log is `scratch/oracle/native.log`.
