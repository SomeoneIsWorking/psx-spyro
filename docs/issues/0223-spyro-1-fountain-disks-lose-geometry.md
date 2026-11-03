# 0223 — Spyro 1: the Artisans fountain disks lose geometry

Status: open (2026-11-03). The report's view is not reproduced; native depth shows a related defect there, so depth
defaults off. Owner: psxport native depth (`docs/presentation.md` Depth) for the pedestal; unknown for the report.

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

## Next
Native depth for an `Object` needs the guest's own "drawn over what it stands on" rule before it defaults on: an
object the guest sorts in front of its floor must not lose pixels to that floor. Place Spyro on the pond's near rim
facing the waterfalls (the report's view) and name the producer that draws the disks.
