# 0224 — Spyro 1: the Toasty portal's sky covers its left inner jamb

Status: open (2026-11-04). Owner: not yet traced; candidate `titles/spyro1/render/portals.cpp` (`Portals`).

## Observed
Player report `20261031-174318-geometry-disappering`, pad frame 3009, mark x 27-39% y 15-52%: the left side of the
Toasty portal arch disappears. The report's `psx.png` is a later frame than `screen.png`; its motion frames show the
jamb steady.

## What was checked
`warp Artisans/Toasty/portal`: in the record path at judgement settings (`scratch/depth/runs/toasty`) and in the device
path (`toasty-dev`) the portal sky covers the left inner jamb when the camera is left of the portal's centre. Depth on and
off draw it the same (`toasty-on/cmp0.png`): the portal sky is not a marked producer and the jamb is drawn before it, so
this is not painter's order between world meshes. At 4:3, ires 1, depth off, the record and device VRAM of one
portal frame from the same run (`scratch/depth/runs/recdev`) differ in 1,252 of 122,880 pixels by more than 8 levels,
scattered along edges, and match in the portal sky: the cover is the guest's own draw, not a record divergence. The jamb face is back-facing from that side, so the cover may be
retail's; it is not yet compared against a frame where the report saw it whole.

With depth on by default, the walk's Toasty portal arrival (`warp 0/4/1`, `scratch/depth2/runs/walk-on2` against
`walk-off`) differs from depth off in 0 of 2,073,600 pixels: native depth does not touch the portal.

## At the report's camera
New reports from the same scene (`scratch/depth3/r0224-sheet.png`, `r0224b-sheet.png`: report screen, our screen,
our device picture) put `screen.png` and `psx.png` on one frame. Over 8 frames at that camera, the native portal and the
original guest portal code (`PSXPORT_OVERRIDE_DIFF`, runs `pseq-orig` and `pseq-nat`) give identical device pictures,
and both jambs draw (`scratch/depth3/pseq.png`). In the `r0224b` frame, sky covers the right inner jamb in both record
and device; that frame was not run against the original.

## Next
Run the `r0224b` state against the original portal code. If they match, the cover is retail's and this closes.
