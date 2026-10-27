# 0214 — Spyro 1 aborts in a moby in-between that patches the near polygon list

Status: fixed (2026-10-28). Owner: `titles/spyro1/render/actor_emitter_producer.cpp` (`ActorEmitterProducer::render`).

## Observed
`tools/oracle_compare.py --route <walk.pad> --frame-step 1` (fps60 from `tools/shipping_settings.ini`, hidden window)
aborted the product at REPL frame 7513, screen key 0x37:
`[emit-memory:error] a render wrote 4 bytes to 0x800728F4, outside its host ranges`, from
`ActorEmitter::patchNear` <- `ActorEmitter::draw` <- `ActorEmitterProducer::render` <- `FrameComposer::place`.
A player at fps60 hits the same abort wherever this moby is drawn near the camera.

## Cause
`patchNear` (0x8001F910) saves each near detail run's old words from `kPatchSaves` (work block + 0x2A00) on. The
in-between's host memory mapped the work block only to + 0x2C00 (the sprite queue's colour table), and a frame with
0x6A4 bytes of runs saves past it: the first save past + 0x200 is the abort at 0x800728F4. The guest itself writes there
freely; only the in-between's map was short.

## Fix
`nearPatchSaveBytes` walks the frame's runs with the same walker `patchNear` uses (`forEachNearRun`), and
`ActorEmitterProducer::render` maps that many zeroed bytes at `kPatchSaves` before the emitter runs.

## Shown
The same oracle route ran the product from boot past REPL frame 100,000 (ten attract-demo cycles through the 7513 scene)
with no `emit-memory` error, where every run before the fix aborted at 7513. The run's last 200 log lines are
`scratch/oracle/native_tail.log`.

## Reproduction
`scratch/player-bugs-3/vib/walk_taps.pad` with the command above.
