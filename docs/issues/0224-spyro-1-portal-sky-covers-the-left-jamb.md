# 0224 — Spyro 1: the Toasty portal's sky covers its left inner jamb

Status: open (2026-11-03). Owner: not yet traced; candidate `titles/spyro1/render/portals.cpp` (`Portals`).

## Observed
Player report `20261031-174318-geometry-disappering`, pad frame 3009, mark x 27-39% y 15-52%: the left side of the
Toasty portal arch disappears. The report's `psx.png` is a later frame than `screen.png`; its motion frames show the
jamb steady.

## What was checked
`gate-teleport 0 0` in Artisans: in the record path at judgement settings (`scratch/depth/runs/toasty`) and in the device
path (`toasty-dev`) the portal sky covers the left inner jamb when the camera is left of the portal's centre. Depth on and
off draw it the same (`toasty-on/cmp0.png`): the portal sky is not a marked producer and the jamb is drawn before it, so
this is not painter's order between world meshes. The jamb face is back-facing from that side, so the cover may be
retail's; it is not yet compared against a frame where the report saw it whole.

## Next
Reproduce the report's camera (replay diverges, issue 0225) and compare the device frame at the same camera before
naming the cause.
