# 0221 — Spyro 2: a moby at the frustum edge loses its polygons in the 16:9 margins

Status: open. Owner: the moby drawer `FUN_80044504` (SCUS_944.25), which still runs as guest code.

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
Port `FUN_80044504` natively (it also needs a producer for issue 0176's keying) and take its outcode window from
`presentedBounds`, as the terrain and sky drawers do. Patching the guest's compare constants would write guest code
and is not done. Spyro 3's moby drawer is not yet checked for the same window.
