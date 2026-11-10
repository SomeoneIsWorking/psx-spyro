# 0236 — Spyro 1 Stone Hill ram: the in-between shows the later pose

Status: closed (2026-11-10). Owner: `game/render/field/keyframe_blend.cpp:between`.

## Observed
Warp `0/1/0`, Up held 30 steps, hidden, unpaced, judgement settings. The ram in front of the first wall
(crop 2600,300..2900,520 of the 5472x1920 dump) bobs its head; an in-between around its charge start is
close to the real present after it rather than halfway (`scratch/replay3/walk5/ram8.png`).

## Cause
The ram is moby `0x801737B8`, class 121. As it starts its charge one tick steps its pose from anim 0 frames
0x16->0x17 at 48/64 to anim 0 frame 0x18 -> anim 1 frame 0 at 16/64: the animation ran through frame 0x17 and
0x18 inside the tick. `keyframe_blend::continues` only knew a pose one pair on (`to`'s frame A = `from`'s frame
B), so this went to `across`, which blends `from`'s frame A toward `to`'s nearer frame: at t=1 it drew 0x16->0x18
at 63/64 instead of `to`, so the t = 1 replay was not the real frame (null case failed in the ram's box only,
`scratch/ram0236/w7`), and at t=0.5 it drew 0x16->0x18 at 56/64 instead of the path's midpoint.

## Fix
`between` follows a pose through whole frames of `from`'s animation (`framesPast`): the path runs
from `from`'s share, through each pair the tick stepped past, into `to`'s pair, and t picks the pair and step
on it. Only a real animation switch goes to `across`. A looping animation wraps on that path at its own
loop (`MobyRecordMotion::loopOf`): Spyro 1's stepper 0x800522C0 sets the next frame to 0 once it reaches the
animation header's frame count (s16 at +0), and Spyro's parts loop between the start and end bytes of their
0x8006C4A0 row (0x8003CB24); Spyro 2/3 mobys loop over the header's frame count byte. Without a loop a pose only
counts forward. The loop is read only for a pose that stepped: a model that never animates has no headers.

## Proof
`scratch/ram0236/` (`run.py`, `PSXPORT_PROBE_REPLAY_T` was a local probe, not shipped):
- t = 1: every in-between equals its real frame, full frame (`n1`); before, the switch interval differed in
  the ram's box.
- t = 0: every in-between equals the real before it in the ram's box (`n0`).
- t = 0.5 (`h1/ram8.png`, `horns.py`): across the charge start the horns sit halfway (left horn x 91.0 / 86.1 /
  79.9, right y 70.3 / 65.9 / 62.4). Real frames with fps60 off are identical to the fps60 run's real frames.
- What still steps there is the camera's yaw, a whole 12-bit unit at a time in every in-between: issue 0238.
- A wrap the guest takes inside one pair (Stone Hill mobys: `(0x2F, 0x00)` then `(0x00, 0x01)`) pairs as
  before (`scratch/review5/mobyprobe.out`). A tick that steps across the wrap needs the stepper to run twice
  (`g_DeltaTime` >= 3); no unpaced or paced run here produced one, so that path is not shown in a run.
