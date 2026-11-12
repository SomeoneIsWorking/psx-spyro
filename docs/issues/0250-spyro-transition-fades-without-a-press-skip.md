# 0250 — Spyro transition fades and flights that a press does not end yet

Status: open; the Spyro 2/3 level fade-in, the fade-out loop before a load, Spyro 2's load hold, its
Exit Level fade and its portal flight end on a press. Owner: `game/core/counted_screen_skip.cpp`
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

- The fade-out loop before a load (Spyro 2 `0x8004D810`, from state 5's draw `0x800151F0` and the ending's
  update `0x800159C8`; Spyro 3 `0x80020168`, from state 5 sub-state 3 in `0x80059038`): 16 passes inside one call,
  each darkening the drawn buffer and waiting in `VSync(0)`, the pass count in `$s0` (`slti $s0, 0x10` at
  `0x8004D948` / `0x800202A0`). The frame driver already suspends the call at that wait, one field per step, so no
  override is needed: a `spyro::CountedLoop` moves the suspended `$s0` (`ResumableGuestCall::setSuspendedRegister`)
  and the loop's own test leaves after the kept passes. Spyro 3 `6 -> 14` and `7 -> 13`, Spyro 2's title fade
  `8 -> 14` and `9 -> 13`, its picture fade likewise (`scratch/skip0250/sheet_s3loop_*`, `sheet_s2loop_*`). Paced:
  `warp 0/1/0` start to gameplay Spyro 3 2.475 s -> 2.325 s, Spyro 2 2.292 s -> 2.124 s (both loops), p50 16.75 ms;
  the worst interval is the load hold, present without a press.
- Spyro 2 state 7 (`0x80019C40`) sub-state 0 has two courses on `0x800698EC`. After Exit Level
  (`0x80019928(-1)` sets `0x800698F8`): +2 per update, the fade `(count + 1) * 8`; `12 -> 28` and `14 -> 26`
  (`sheet_s2exit_*`); Glimmer to Idol Springs arrival 30.124 s -> 29.857 s. After a portal (`0x80019928(index)`,
  the type-6 trigger at `0x8002AD98`): +1 per update to 0x20, the camera eased from `0x8006B2D4` to `0x8006B2E0`
  by count / 32 and Spyro moved by `0x8006B2C4`; `11 -> 29` and `12 -> 30`, both leaving on the same step
  (`sheet_s2flight_*`); Summer Forest to Glimmer arrival 10.765 s -> 10.164 s. Nothing persistent depends on where
  the flight ends: sub-state 2's load resets Spyro to (0x2000, 0x2000), and sub-state 4 reads the flight words
  `0x8006B2B8` only on arrival in a homeworld (`0x80066EE4`), which a portal from a homeworld never is; the
  Glimmer arrival is (0x10C66, 0x818F, 0x2667), camera 0x1140A, with or without the press. The `warp` from a
  homeworld now enters a level through its own portal (`spyro2::Warp::changeLevel`).

- Spyro 3 state 7 (`0x80054450`) runs the same two courses on `0x8006FBC4`, entered by `0x80053F50` (-1 from the
  pause menu's Exit Level at `0x80058378`, an index from the type-6 trigger at `0x80040230`); the flight runs only
  while both `0x8006FBD0` and `0x8007056C` are zero. Exit Level fade `12 -> 28` and `14 -> 26`
  (`sheet_s3exit_*`), Sunny Villa to its homeworld portal 11.099 s -> 10.870 s (8 frames); portal flight `11 -> 29` and
  `12 -> 30`, Sunrise Spring to Sunny Villa 10.935 s -> 10.297 s, arrival (0xF8A4, 0x533D), camera 0xF8A4, with
  or without the press (`sheet_s3flight_*`). Spyro 3's `warp` enters a level from its homeworld through its
  portal, and its `portal` arrival leaves through Exit Level, both via `spyro::portalInto` / `0x80053F50`.

## Not wanted
- The fade-out and respawn hold after a death (sub-state 0 and 1 while `0x800698F8` / `0x8006FBD0` is set): not
  skipped, per the user, 2026-11-12.

## Not done
- Spyro 2 state 7 sub-states 1-3 (overlay `0x80071BD4`) and Spyro 3's sub-states 1-5 (overlay `0x80076538`): the
  portal's sky and load, about 290 / 260 updates after the flight; Spyro 2's state 6 arrival flyby (Idol
  Springs: about 700 updates). Not traced.
- Spyro 1's balloonist (overlay `balloonist.inc.h`, `D_800777E8` at `0x800777E8`, tick `+4` by `g_DeltaTime`):
  state 5 places the camera from the tick alone while `LoadLevel(1)` runs a stage per frame and leaves at tick
  0x100 once stage 13 is reached, so a press can bring the tick to its end; states 2, 3 and 6 move the balloon per
  update and would land it elsewhere. No debug route reaches it yet. The conversation starts through
  `g_BalloonistTriggerCode` (`D_800757A0`, set per homeworld in `overlay_pointers.c`, called from the
  balloonist's moby update), and its choices (dialogue 30: homeworld list, 31/32: yes/no) are taken in state 1
  from `g_Pad.m_Down` (`0x80077378`), which the guest's own frame update fills from the pad buffers before the
  gamestate dispatch. A route through the game's own choice therefore needs a seam between that pad read and
  `func_800324D8`, or the choice's Cross delivered through the pad buffers; neither exists yet.
- Menus and cutscenes: Spyro 2's story scene (state 6, level 0x48) already ends on the guest's own Start;
  the pause menu's and the S1 menus' fades are not traced.

## Cause
Each runs on its own counter or its own frame loop; none reads the pad. A fade that loops inside one update call
(Spyro 3 `0x80020168`) has no frame boundary where a press could act, so it needs its loop owned before a press
can end it.

## Proper fix
Per screen: decompile its owner, find the count or tick its own test leaves on, and on a press bring it to the
value the guest's next update leaves from, keeping the draw buffers whole (issue 0248: two full frames and an
even number of dropped updates). Prove each with frame sheets at both press parities and a paced run, as 0248
does. A screen with no such exit stays unskippable and is recorded here.
