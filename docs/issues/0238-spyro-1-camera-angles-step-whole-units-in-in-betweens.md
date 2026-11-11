# 0238 — Spyro 1: the camera's angles step whole 12-bit units in in-betweens

Status: closed (2026-11-11). Owner: the camera track (`titles/spyro1/core/frame/draw_motion.*` `CameraMotion`)
and the section's camera step (`draw_section.cpp`).

## Observed
Warp `0/1/0`, Up held (`scratch/ram0236/h1`): while Spyro walks the camera yaw moves one unit (0x409 <-> 0x40A,
0x40A -> 0x40C) on some ticks. The distant scene (gate, walls, the ram) jumps about 4 px at 8x on those ticks
and the in-between shows one end, not the middle (left horn x 94.7 / 94.3 / 90.8 across one interval).

## Cause
`g_Camera` +0x4C..+0x50 are 12-bit angles and `Motion::Angle12x2`/`Angle12Low` blend them to whole units
(`turnedHalf`, `lerpInt`); CameraUpdateMatrices 0x80033C50 builds the matrices from the integer angles through
Sin 0x80016C58 / Cos 0x80016CB0 (table 0x8006CBF8, index `(a & 0xFFF) >> 4`, the low nibble interpolating to
the next entry), so a half unit is lost.

The near sky through a portal (native 0x80050BD0, `portals.cpp:nearSkyRotations`) also passed the whole
halfwords to `guest_camera::Builder`, which reads its angles with `kFineBits` below the unit, so it turned by a
sixteenth of the camera's angle. The guest's 0x80050BD0 calls Sin/Cos with the whole angles.

## Fix
`CameraMotion` blends m_Rotation's three angles the short way round with four bits below the unit, writes each
halfword's nearest whole unit for the guest, and notes the fine angles in `NativeState::replayCamera` for the
replay. After the replayed 0x80033C50 the section writes both matrices (nine halfwords each, the pad left as the
guest wrote it) from `TerrainImage::camera` at the fine angles: the guest's own Sin/Cos interpolation and
MulMatrix order, which at whole angles is the guest's result. `TerrainImage::cameraAngles` gives every reader
the angles in the builder's unit (the replay's fine angles while one draws), so the near sky takes the same
camera and its spin in that unit.

## Proof (2026-11-11, `scratch/camfrac/`, judgement settings, hidden, unpaced)
- Turning, warp `0/1/0`, Up+Left held (`turn2`, `shift.py`, `turn_castle.png`): the camera yaw moves 18 to
  21 units a tick and the castle band moves 47 / 46, 54 / 53, 56 / 56 output px across each tick's
  in-between and real. The 21-unit tick splits evenly, which whole units (10 / 11) could not.
- Walking, Up held (`walk1`, `edge.py`): the tower's edge in-betweens lie within 1.2 output px (0.15 native
  px) of their neighbours' midpoint; the 4 px jumps are gone.
- t = 1 (local probe, `t = 1.0f` in ReplayFrame, not shipped): the same 12 presents with the camera matrices
  written and with that write disabled are pixel-identical (`t1turn`, `t1turnB`). Title menu, picker and
  leave in-betweens equal their reals byte for byte (`t1title`, 20 of 20).
- The near sky turns with the camera: Toasty's portal (`warp Artisans/Toasty/portal`, Right held,
  `portal3`) shows its clouds moving about 112 px while the portal frame moves 150 px.
- Leak: same inputs with fps60 on (1878 in-betweens) and off, `fielddigest` identical over 4576 fields
  (`leak-0/1`); the title route, 5709 fields (`tleak-0/1`).
- Paced, host load 2.8-3.7: 698 of 701 simulated 60.00 Hz vblanks advance 0.5 tick, 3 repeats (`paced1`).
- Run-end: 0 intervals refused; camera 0 of 1715 record draws unpaired.

A t = 1 in-between still differs from its real in a few small yellow squares beside a gem's sparkle in Stone
Hill (`t1turn`, bounding box about 360 x 100 at 8x), with the camera write and without: 0239.
