# 0221 — Spyro 2: a moby at the frustum edge loses its polygons in the 16:9 margins

Status: fixed 2026-11-03. Owner: the moby walk's class (`guest_moby_visibility.cpp:MobyWalk::edgeTestsCullMargin`);
the edge tests themselves are the guest drawers' (`FUN_80044504` SCUS_944.25, `FUN_80031124` SCUS_944.67).

## Observed
Spyro 2 from boot, `ires=0`, `fps60=1`, 16:9, sink 3840x2160, unpaced. In the boot intro Spyro flies past the left edge
at about column 65 of 684, inside the left margin. Real present 592 (fence 1089) draws him whole. Real present 593
(fence 1092) draws only his right wing and a foot in the same place; the body, head and left wing are gone, although
he has hardly moved and neither has the camera. At fence 1101 a left wing piece comes back. The in-betweens at
1092 and 1101 follow the next real present, at shares 0.94 and 0.73 (`scratch/filed-defects-2/0220/clip/held.txt`,
crops `scratch/filed-defects-2/0220/judge/zoom1092.png`, `zoom2.png`, `zoom1101.png`).

## Cause
The walk classes a record 2 (inside) or 1 (edge) against the widened slope (`guest_moby_visibility.cpp:drawCandidate`).
Spyro's record (array 0x80093BCC, mesh 0) is class 2 through field 592, then class 1 from field 593 as it starts to
move (`judge/records.txt`, byte +0x4D). A class-1 entry takes the drawer's clipped path. That path gives each
projected vertex outcodes against retail's window at 0x80044BB0..0x80044BEC: y <= 0x0D, y >= 0xE4, x <= 0 and
x >= 0x200. Polygons whose vertices all lie past one edge are then dropped. At 16:9 the left margin is negative native
x, so the parts of an edge-class moby that lie in the margin are culled. Class-2 entries are not tested, so the same
moby draws whole until its sphere touches the widened edge. This is a horizontal limit the title owns that was never
widened (issue 0154's rule). It is unrelated to the in-between: both real presents show it.

## Fix
Both drawers' class-1 path only rejects polygons whose vertices share an outcode; it never clips. So the
walk, which decides the class, lists a moby as inside (2) when the view is widened, its sphere is clear of the
eye and it reaches past retail's horizontal window: the 4:3 edge tests would only drop margin polygons, and the
GPU clips what lies off the canvas. The record's +0x4D keeps the frustum class. At 4:3 nothing changes. Spyro 3's
drawer (`FUN_80031124`) has the same window (0x0D/0xE4/0/0x200) and takes the same walk.

A moby whose sphere crosses the eye plane stays class 1 (its behind-eye polygons must still be rejected), so its
margin polygons can still drop. Porting the drawers natively with `presentedBounds` would close that too.

## Check
Spyro 2 from boot, `ires=0` (8x), `fps60=1`, sink 3840x2160, unpaced, dumps of fences 900-1200
(`scratch/fix-0221/`). Spyro is whole in every real present and in-between through 1089-1104
(`strip1092.png`, `strip1101.png`, `wing-triples.png`); the in-between at 1092 now sits 0.59 of the way (was
0.94). 96 of 100 in-betweens lie 0.38-0.63 (`held.txt`); 1095 (0.30) and 1101 (0.74) are Spyro's eye-blink
texture swap, which has no in-between pose, with body and wings between their neighbours; 1086/1089 have no
motion. Spyro 3 from boot at the same settings draws Spyro whole in the left margin at fence 1518
(`keep-s3-1518.png`).
