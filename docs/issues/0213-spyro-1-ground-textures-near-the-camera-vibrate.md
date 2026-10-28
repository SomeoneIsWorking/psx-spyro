# 0213 — Spyro 1 ground textures near the camera vibrate

Status: closed, retail behaviour (2026-10-28, console compared). Owner: the guest's camera follow.

## Observed
Player report `20261023-181541-textures-close-to-camera-may-vibrate-when-walkin`, pad frame 5285, mark x 36-58%
y 79-94%: the path stones right in front of the camera shimmer while Spyro walks.

## Reproduction
`scratch/player-bugs/run_replay.py <report> vib4 2700 PSXPORT_NOPACE=1 PSXPORT_FPS60_DUMP_FROM=2610
PSXPORT_DEBUG=fps60dump PSXPORT_SETTINGS=tools/shipping_settings.ini` dumps every present, in-betweens included,
over the walk (`FramePresenter::dumpPresent`, 5472x1920 record pictures at 8x). Pad frame 5285 is fence 2687; at fence
2670 the replay matches the player's screenshot (`vib4/full_vs_player.png`). `vib4/crops.py` crops the mark from each
present; `vib4/crop_steps.txt` is the per-present change, `vib4/strip_2651_2654.png` eight consecutive presents.

From fence 2675 on Spyro stands still and the crop does not change (52 presents), so the recording's last frames are
not a walk: the player stopped before pressing the report key. That is why the earlier five pad shots 5282-5286 were
identical. During the walk (fences 2610-2674) every present moves; none is held, so this is not a pacing repeat.

## Cause
The guest camera itself oscillates. Logging the terrain producer's saved camera per real field
(`readCamera`: angles and position, `scratch/player-bugs-3/vib/camera_fields.log`) over the walk shows the
position reversing every field whenever Spyro slows or stops, `(3,2,2) (-4,-3,-2) (3,2,2) (-3,-4,-2)`, and the
yaw alternating by one unit (3473/3474). That is the guest's camera follow settling into a one-field limit cycle; it
is guest state written by guest code, so retail should draw it on its own 30 fps frames; this is inferred, not
measured on the console.

What the port adds is halfway and nothing else:
- the rebuilt matrices equal the guest's at every real field (`image.camera` against `0x80076DD0`, 0 words differ);
- each in-between's camera is the midpoint of its two fields (angles and position as logged);
- at fps60 off and 1x (`vib/a30crops`) the same near stones reshape on real frames alone.

The near stones' texel grid reshaping is the PSX's affine texture mapping of large near polygons under that
camera motion, which the Record renderer reproduces.

## Console comparison
`tools/oracle_compare.py --frame-step 1` route through Artisans, product at 1x with fps60 off
(`scratch/player-bugs-4/probe/camera_route.py`, `camera_frames.py`): the camera words (`0x80076DD0`, 0x110 bytes,
position at +0x28) are equal on both cores on the first 279 route frames, up to gameplay[4] frame 24, after the
console's CD wait at gameplay[3] frame 26 (issue 0215, item 2) changed its `g_DeltaTime`, including
the reversal frames of the straight walk (gameplay[1] frames 38-43, x 84997, 85005, 84994, 85007). The console's own
frames over that walk (`cam2/near_seq_37_44.png`, product frame N beside console frame N+1, since the console parks
before it draws) show the same near-ground texel reshaping. The console draws it the same way: retail.

Not part of this issue: at 1x the product's ground is 3-5 levels brighter per channel than the console's and has no
dither pattern (`cam2/`, mean RGB 71/143/56 against 68/139/53). The console's frames carry 5 bits per channel (low three
bits always zero) and the product keeps full precision without dither, by design (psxport `docs/presentation.md`,
Raster); dropping the product's low three bits lowers its mean by 2.4-3.9 per channel, which is the gap.
