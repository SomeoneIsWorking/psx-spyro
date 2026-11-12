# 0250 — Spyro transition fades and flights that a press does not end yet

Status: open, not worked. Owner: none yet; each is title-owned beside `titles/spyro1/core/frame/transition_skip.cpp`
or `game/core/loading_picture_skip.cpp`.

## Observed
Start/Cross ends the boot logo holds (all three titles), Spyro 1's loading music fade (issue 0249) and the Spyro 2/3
loading picture (issue 0248). These other non-player screens still run to their own end with a press:

- Spyro 2 level change, game state 3 (`0x8001722C`): sub-state 0 fades the level out, 1 loads, 2 fades the next
  one in. Spyro 3's equivalent fade-out is `0x80020168`, called from the loading picture's sub-state 3: one call
  runs all 16 fade frames itself (its own `VSync`/`DrawOTag` loop), then sets both draw envs' clear flag.
- Spyro 1 balloonist flight (overlay `balloonist.inc.h`, states 4-6 on the tick `D_800777E8.unk_0x04`): the
  camera arc runs while the tick is below 0x100 in state 5 and the arrival while it is below 0x80 in state 6.
- Portal entry and exit in all three titles, and the remaining fades into and out of menus and cutscenes. None
  of these is traced yet.

## Cause
Each runs on its own counter or its own frame loop; none reads the pad. A fade that loops inside one update call
(Spyro 3 `0x80020168`) has no frame boundary where a press could act, so it needs its loop owned before a press
can end it.

## Proper fix
Per screen: decompile its owner, find the count or tick its own test leaves on, and on a press bring it to the
value the guest's next update leaves from, keeping the draw buffers whole (issue 0248: two full frames and an
even number of dropped updates). Prove each with frame sheets at both press parities and a paced run, as 0248
does. A screen with no such exit stays unskippable and is recorded here.
