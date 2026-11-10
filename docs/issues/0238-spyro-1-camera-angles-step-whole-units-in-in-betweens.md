# 0238 — Spyro 1: the camera's angles step whole 12-bit units in in-betweens

Status: open. Owner: the camera track (`titles/spyro1/core/frame/draw_motion.h` `kCamera`) and the camera builder
0x80033C50 the section runs.

## Observed
Warp `0/1/0`, Up held (`scratch/ram0236/h1`): while Spyro walks the camera yaw moves one unit (0x409 <-> 0x40A,
0x40A -> 0x40C) on some ticks. The distant scene (gate, walls, the ram) jumps about 4 px at 8x on those ticks
and the in-between shows one end, not the middle (left horn x 94.7 / 94.3 / 90.8 across one interval).

## Cause
`g_Camera` +0x4C..+0x50 are 12-bit angles and `Motion::Angle12x2`/`Angle12Low` blend them to whole units
(`turnedHalf`, `lerpInt`); 0x80033C50 builds the matrices from the integer angles through the sine table, so a
half unit is lost.

## Next
Give the camera builder the angles' fractions the way `ReplayAngles` gives the moby list builders theirs: a
native 0x80033C50 that composes from fractional angles (the arithmetic is `game/render/terrain/guest_camera_builder.h`),
or write the builder's matrices from blended angles after it runs in the replay.
