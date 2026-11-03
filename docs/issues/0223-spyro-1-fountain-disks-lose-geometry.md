# 0223 — Spyro 1: the Artisans fountain disks lose geometry

Status: open (2026-11-03), not reproduced. Owner: unknown until reproduced.

## Observed
Player report `20261031-174434-these-circular-disks-have-geometry-that-disappea`, pad frame 6413, mark x 47-58%
y 26-34%: the stepping disks in the pond below the Artisans waterfalls lose part of their geometry. The report's
`psx.png` is a later frame than `screen.png`, so the two cannot be compared at the mark.

## What was checked
The report replay reaches a different place at 6413 (issue 0225). `dragon-teleport 3` (Delbin, by the pond), then
700 frames, shows the disks from behind the rescue spot (`scratch/depth/runs/delbin-on/disks_cmp.png`): with depth on
and with `PSXPORT_DEPTH=0` both disks draw whole and the same, so at that view they are not a painter's-order defect.

## Next
Place Spyro on the pond's near rim facing the waterfalls (the report's view) and walk the camera around the disks;
record which producer draws them before naming an owner.
