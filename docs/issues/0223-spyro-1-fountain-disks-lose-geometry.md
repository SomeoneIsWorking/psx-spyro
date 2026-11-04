# 0223 — Spyro 1: the Artisans fountain disks lose geometry

Status: open (2026-11-04). The depth defects at the fountain are fixed and depth defaults on. Near the report's view,
depth on cuts the disks at the waterline; the report's exact camera is not reached. Owner: psxport native depth (`docs/presentation.md` Depth) for the pedestal; unknown for the report.

## Observed
Player report `20261031-174434-these-circular-disks-have-geometry-that-disappea`, pad frame 6413, mark x 47-58%
y 26-34%: the stepping disks in the pond below the Artisans waterfalls lose part of their geometry. The report's
`psx.png` is a later frame than `screen.png` (the camera differs), so the two cannot be compared at the mark. The
report's build had no native depth.

## What was checked
The report replay reaches a different place at 6413 (issue 0225). `warp Artisans/Artisans/dragon 4`, then 1000 frames
(`scratch/depth/disks-d1.txt`, runs `disks-d1` and `disks-d0`, ires 8x, 16:9, unpaced):
- The pond disks draw whole and the same with depth on and off, and match the device VRAM of the same frame
  (`disks-d1/dev2.png`). At that view they are not a painter's-order or record-vs-device defect.
- With depth on, the pedestal in front of the pond loses its outer rim and left tip to the grass
  (`disks-d1/pedestal.png`, on above off). The pedestal mesh sits partly below the ground; retail draws it over the
  ground by slot order, true depth hides the buried part.
- With depth on, a river band shows in the right 16:9 margin where slot order draws the grass over it
  (`disks-d1/rightbank.png`); the device's 4:3 frame does not cover that area.

## Fixed
- Pedestal rim. The guest sorts a moby into one OT slot and draws its parts in order, so nothing slot order put before
  it covers it. `DepthOrder::Object` now draws over the scene without testing and writes its depth
  (`psxport runtime/psx/render/record_rasterizer.cpp`, one ALWAYS+write pipeline); later polygons test against it.
- River band in the right margin. `titles/spyro1/render/terrain_far.cpp:FarPass::run` projected the far mesh with no
  depth unit, so its SZ in world >> 4 read as near view units, 4x too near, and the far river beat the near grass. It
  now opens `DepthUnit(2^(kFarViewShift - kNearViewShift), -(kBinBase << kDepthBinShift))`: the far pass's scale, and
  the 0x40 bins it sorts behind near terrain as a farther offset.

Shown at `warp 0/0/4` (`scratch/depth2/fountain-right.png`: before, depth on at the old head, depth on now): the
pedestal keeps its rim and the margin shows grass, as with depth off. The arrival walk's `dragon 4` shot differs from
depth off in 74 of 2,073,600 pixels, none on the disks or the pedestal.

## Near the report's view
`warp 0/0/4`, 1500 fields, `spyro-at 72100 43766 6500 2048` and `72100 45000 6500 2048`, 300 fields
(`scratch/depth3/fprobe2.txt`, runs `fprobe3` and `fprobe3-off`): the pond, both waterfalls and the stepping disks face
the camera, as in the report but from further right. With depth off the disks' stone sides draw down to the rim over the
water. With depth on the water covers each side below the waterline (`scratch/depth3/r0223-disks.png`: off, on, at two
placements). That is the waterline, so the slot order was wrong there. The report (no depth) shows the water cutting the
disks' sides from its own camera. Retail's polygon sort puts the water over or under a whole disk side depending on the
view, which is the reported disappearing.

## Next
Reach the report's camera (further left, the crystal dragon statue in view) and compare depth on there.
