# 0213 — Spyro 1 ground textures near the camera vibrate

Status: retail behaviour (2026-10-28). Owner: the guest's own camera follow; not a port defect.

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
is guest state written by guest code, so retail draws it on its own 30 fps frames.

What the port adds is halfway and nothing else:
- the rebuilt matrices equal the guest's at every real field (`image.camera` against `0x80076DD0`, 0 words differ);
- each in-between's camera is the midpoint of its two fields (angles and position as logged);
- at fps60 off and 1x (`vib/a30crops`) the same near stones reshape on real frames alone.

The near stones' texel grid reshaping is the PSX's affine texture mapping of large near polygons under that
camera motion, which the Record renderer reproduces.

## Not compared
The console reference was not run on this replay: a pad replay recorded on the product does not stay aligned
through the console's slower loads, so the console frames at pad frame 5285 are not the same moment. A held-input
walk on the console from a warp would confirm the same per-field camera reversal in retail.
