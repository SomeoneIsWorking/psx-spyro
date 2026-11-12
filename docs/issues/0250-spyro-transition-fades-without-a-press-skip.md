# 0250 — Spyro transition fades and flights that a press does not end yet

Status: open; the Spyro 2/3 level fade-in and Spyro 2's load hold end on a press. Owner: `game/core/counted_screen_skip.cpp`
(Spyro 2/3) and `titles/spyro1/core/frame/transition_skip.cpp` (Spyro 1).

## Done
The level load (Spyro 2 state 3 `0x8001722C`, Spyro 3 `0x80053A10`) is a `spyro::CountedScreen` per step, on the
counter `0x800698EC` / `0x8006FBC4` and the fade word `0x80066F68` / `0x8006C598` (both `lw`/`sw`):

- Fade-in (Spyro 2 sub-state 2, Spyro 3 sub-state 4): -0x10 per update until below 1. A press keeps the last two
  or three updates, so both buffers are drawn: Spyro 2 `191 -> 31`, `175 -> 47`, Spyro 3 `175 -> 47`, `159 -> 31`
  (`scratch/fades/s2fi1`, `s2fi2`, `s3fi1`, `s3fi2`). Paced at `ires=0`, `fps60=1`, 3840x2160 (`scratch/fades/paced_*`):
  `warp 0/1/0` start to gameplay Spyro 2 2.325 s -> 1.957 s, Spyro 3 2.459 s -> 2.190 s, p50 16.75 ms in all four.
- Spyro 2's load hold (sub-state 1, black, ++ once the load is issued, leaves past 5): `2 -> 4`
  (`scratch/fades/s2h2`). A press on a screen that is not offered is dropped, not carried to the next (`s2h1`).

## Not done
- The fade-out (sub-state 0, +0x10 until past 0xFE) and the respawn hold (sub-state 1 while `0x800698F8` /
  `0x8006FBD0` is set, to 0x4F) run only after a death (Spyro 2 `0x8004C1AC` from `0x80032E74`); no debug route
  reaches a death, so they are not offered.
- Spyro 2 state 7 (`0x80019C40`): the level-exit flight, its camera moved per update and the fade `(count + 1) * 8`
  over `0x800698EC` to 0x20; reached by `warp` from play (`scratch/fades/spyro2_trace.log` 1151-1166). State 6 after
  it is the arrival flyby (fade 0xF8 down by 0x20).
- Spyro 3 `0x80020168` runs all 16 steps of its fade-out inside one call (its own `VSync`/`DrawOTag` loop); a press
  needs an override that owns the function and leaves its loop, as the logo holds do.
- Spyro 1's balloonist (overlay `balloonist.inc.h`, `D_800777E8` at `0x800777E8`, tick `+4` by `g_DeltaTime`):
  state 5 places the camera from the tick alone while `LoadLevel(1)` runs a stage per frame and leaves at tick
  0x100 once stage 13 is reached, so a press can bring the tick to its end; states 2, 3 and 6 move the balloon per
  update and would land it elsewhere. No debug route reaches a balloonist yet (its dialogue is a player choice).
- Portal entry and exit, and the fades into and out of menus and cutscenes: not traced.

## Cause
Each runs on its own counter or its own frame loop; none reads the pad. A fade that loops inside one update call
(Spyro 3 `0x80020168`) has no frame boundary where a press could act, so it needs its loop owned before a press
can end it.

## Proper fix
Per screen: decompile its owner, find the count or tick its own test leaves on, and on a press bring it to the
value the guest's next update leaves from, keeping the draw buffers whole (issue 0248: two full frames and an
even number of dropped updates). Prove each with frame sheets at both press parities and a paced run, as 0248
does. A screen with no such exit stays unskippable and is recorded here.
