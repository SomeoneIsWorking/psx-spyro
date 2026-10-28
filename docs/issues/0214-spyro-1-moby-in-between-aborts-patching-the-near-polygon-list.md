# 0214 — Spyro 1 aborts in a moby in-between that patches the near polygon list

Status: fixed (2026-10-28). Owner: `titles/spyro1/render/actor_emitter_producer.cpp` (`ActorEmitterProducer::render`).

## Observed
`tools/oracle_compare.py --route <walk.pad> --frame-step 1` (fps60 from `tools/shipping_settings.ini`, hidden window)
aborted the product at REPL frame 7513, screen key 0x37:
`[emit-memory:error] a render wrote 4 bytes to 0x800728F4, outside its host ranges`, from
`ActorEmitter::patchNear` <- `ActorEmitter::draw` <- `ActorEmitterProducer::render` <- `FrameComposer::place`.
The attract demo runs the level's own moby draw, so a player frame with as many near runs would save past the same
bound; that is inferred from `patchNear`, not shown on a player route.

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

## Why the oracle route never left the attract demo
`oracle_spyro1.advance` ended a game frame when `g_UnprocessedFrames` fell at a step boundary, which is how the
console parks (per field). The product's REPL step is a whole logic frame: `FrameDriver::runGuestUpdate` zeroes the
counter and the frame's two fields refill it, so it read 2 at every step end and the barrier ran on for thousands of
steps per "frame" (`scratch/player-bugs-3/title-probe/`). The title's TSM_Init press-start window passed between two
observations, Start was never pressed, and the demo looped. The product's barrier is now
one REPL step after the boot prefix (the prefix's steps run no guest frame, so it steps until `g_DeltaTime`, first
written by the frame update, is nonzero); the console keeps `step_until_counter_resets`. With it the product holds TSM_Init and reaches its press-start sub-state at game frame 599.
`tools/oracle_compare.py --budget 3000` then reaches every checkpoint: the save picker after 688 product game frames
(console 748), GS_Playing, settled play and `gameplay[0]` (`scratch/player-bugs-3/oracle_artisans.log`). The
comparison at GS_Playing still differs on decisive ranges (`game_tick` 3 vs 2, `player.position`, `pad.down`); the
later checkpoints differ only in informational ranges and the SPU. Those are comparison findings for their own issue,
not the route.
