# 0224 — Spyro 1: the Toasty portal's sky covers its left inner jamb

Status: closed (2026-11-04): the original guest portal code draws the same cover. Owner: `titles/spyro1/render/portals.cpp`
(`Portals`, `0x80050BD0`).

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

## Against the original
The `r0224b` report's `state.bin` (`scratch/depth3/runs/r0224b/reports/20261104-002950.draft`) loaded in a fresh process,
30 fields, once with the native portal and once with the original guest code (`PSXPORT_OVERRIDE_DIFF=spyro1-portals`,
`PSXPORT_OVERRIDE_DIFF_FIRST=100000`: the original runs live and the run continues from its state): the two pictures are
equal in every pixel, and both show the sky over the right inner jamb as the report does (`scratch/depth3/s0224-sheet.png`:
report, original). The cover is the guest's own draw. The differential itself reports every sampled call mismatched
(first: `v0`, 21 registers and 21,597 bytes in 2,439 ranges, `scratch/depth3/runs/s0224-orig/diff.json`) with equal
pictures; that is the portal override's state contract, not this picture, and is not traced here.
