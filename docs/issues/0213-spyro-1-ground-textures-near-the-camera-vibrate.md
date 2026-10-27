# 0213 — Spyro 1 ground textures near the camera vibrate

Status: open (2026-10-27). Owner: not yet attributed (terrain split decision, unplaced vertices, or retail's own
affine warp).

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

## Not yet known
The crops show the near stones' texel grid reshaping between presents, but not whether that is the terrain owner
switching a near polygon's subdivision between frames, near vertices presented without their sub-pixel places, or the
PSX's affine texture warp that retail shows too. Telling them apart needs, per present, the guest's subdivision
choice for the polygons under the mark, and the same frames from the console reference (`external/psxport/tools/oracle`)
at 1x to see whether retail swims the same way.
