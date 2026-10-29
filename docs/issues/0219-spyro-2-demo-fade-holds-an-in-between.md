# 0219 — Spyro 2 attract demo holds an in-between at each fade

Status: fixed 2026-10-29. Owner: `game/core/frame_cut.*` (`FrameCut::onFrameTail`).

## Observed
Spyro 2 attract demo, 16:9, `fps60=1`, unpaced, fences 5250-5560 from boot (no input). The in-between at
fences 5265, 5380 and 5491 equals the new frame (`[presentseq] ... cut=1`), so each one shows a
whole-frame step while the camera keeps moving. 5265 and 5491 are the starts of the demo's fade to
black, and 5380 is the end of its fade-in.

## Cause
The flips were not the game's. Spyro 2's `CdControlB` at 0x80058994 takes only `(com, param)`: each of its `CD_cw`
calls passes `a2 = 0` (0x80058A1C, 0x80058A44, 0x80058A68). `titles/spyro2/core/runtime.cpp` bound it as the
framework's three-argument `controlBlocking`, which answers into the caller's leftover `a2`; from the music controller
`FUN_80012CBC` that register held the game-state word, so the answer wrote 3 and then 0 into `0x800681C8`
(`PSXPORT_CW=0x800681C8,0x800681CC`: a store every ~37 frames from 0x80013424, the return of its `jal 0x80058994`).
The demo restarted its level load each time. The console oracle runs the same demo in state 0 from field 4472 to
5464 and then loads the title, with no state 3 in between (`scratch/filed-defects/0219/oracle_states.out`).

State 3 itself is the level load `FUN_8001722C`: sub-state (`0x800698F0`) 0 fades out over the field, 1 loads behind
full black (its draw `FUN_80014504` draws no field), 2 fades in over the loaded field, then state 0. The draw draws
the field at sub-states 0 and 2 with the same call as state 0 (`0x800155A0(0x3D)`).

## Fix
- `0x80058994` is bound as the no-result control entry (`controlFire`), so nothing is written through `a2`.
- `FrameCut` counts a level-load record that draws the field as the field state (`LevelLoadFacts`: Spyro 2 state 3
  at sub-states 0 and 2, Spyro 3 state 3 at `0x8006FBC8` 0 and 4, `FUN_8001DC5C`), so the only cut in a load is the
  first fade-in record, which is fully black.

## Shown
Hidden unpaced boot, `fps60=1`, sink 3840x2160, `PSXPORT_CW` on the state word: after the demo's load it stays in
state 0 for 486 frames and goes to state 5 and the title (console: ~496 frames). `PSXPORT_DEBUG=cut`: one cut at the
black first fade-in record, then 374 records through the fade-in and the demo with no cut.
