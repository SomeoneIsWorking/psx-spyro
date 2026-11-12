# 0217 — The frame loop is not owned

Status: resolved (2026-11-12). Owner: `titles/spyro1/core/frame/frame_driver.cpp, game/core/boot_prefix_frame_driver.cpp, game/core/field_owner.cpp`.

## Cause
Spyro 1 delivered fields as the guest clock crossed them (`FieldOwner::armHostClock`, armed only from
`titles/spyro1/core/frame/boot_sequence.cpp`). `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::runCall`
gave Spyro 2/3 a field per VSync wait and another whenever a call ran out of host turns, with a fresh
one-field budget per call, so a heavy frame's length followed turn counts rather than the guest clock.

## Why the guest's wait stays the cadence
Every draw ends with an inline wait, not a function: optional `VSync(0)` while a flag is set, then
`while (VSync(-1) - last < 2) VSync(0)`. Spyro 2 draw `0x800156FC` (flag `0x80067044`, stamps
`0x80066F9C`/`0x80066FA0`; states 4 and 9 wait once), Spyro 3 draw `0x8001E638` (flag `0x8006C678`,
stamps `0x8006C5D4`/`0x8006C5D8`; state 9 waits once), Spyro 1's state draws (flag `0x80075784`,
stamps `0x80075950`/`0x80075954`). It gives max(2, k + flag) fields, k the fields already elapsed;
`Timing::vsyncFields(2)` would give max(2, k + 1), one too many on an overrun without the flag. So the
driver keeps the guest's loop and owns only the field source.

## Fix
One field source for all three titles: `FieldOwner`'s host turn delivers a field each time the guest
clock crosses one, and each `VSync(0)` (a frame-boundary exit) takes the next. `BootPrefixFrameDriver`
arms the clock at boot and runs one logic frame per step, update then draw, presented when the draw
returns; a boot field, a field of the fade loop before a load (`BootPrefixFacts::fadeFrameWait`) and
each field of a stall past the delta clamp are steps of their own. The turn-budget field path, the
boot step bound and the unused `FieldOwner::drawnFrameFields` / `FieldOwnerFacts::drawWaitFlag` are gone.

## Measured after
`scratch/frameloop/run/` (headless, `PSXPORT_NOPACE=1`, ires 0, fps60 1, sink 3840x2160), each run
the title screen then `warp 0/1/0` at step 800:
- Spyro 2 boot prefix returns after 597 fields, Spyro 3 after 715. Gameplay and title screen: every
  logic frame covers 2 fields (Spyro 2 1,578 Glimmer frames, Spyro 3 1,575 Sunny Villa frames), 29.9
  logic ticks per second; `[playcheck] cadence:` 0.50 logic ticks per 60 Hz vblank on 3,915 of 5,003
  vblanks (Spyro 2) and 4,056 of 5,225 (Spyro 3), the rest the boot and the load.
- Overrun: a Spyro 3 state 5 frame ran 3 fields of guest work before its tail, which then did not
  wait: stamp delta 3 = max(2, 3 + 0) (`vsyncFields` would have made it 4). Spyro 2's level load is
  one 436-field logic frame, stamp delta 436, each field past the clamp presented.
- fps60 on and off: field digests joined on `ticks=` identical, 2,987 of 2,987 fields (Spyro 2) and
  3,209 of 3,209 (Spyro 3), through the warp and the load.
- Paced walks (`PSXPORT_NOPACE` unset, host loaded by another agent's Clang build, load average 7-18
  on 16 cores): Spyro 1 Stone Hill `presents: missed=36 (1.21%) p99=26.00ms`; Spyro 2 Glimmer
  `missed=177 (6.42%) p99=47.25ms`, the misses the pacer's dropped debt in gameplay (present
  deadlines jumping 50 ms), each present shown within 16.9 ms of its deadline.
