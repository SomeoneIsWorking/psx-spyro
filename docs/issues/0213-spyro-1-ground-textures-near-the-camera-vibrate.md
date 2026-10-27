# 0213 — Spyro 1 ground textures near the camera vibrate

Status: open (2026-10-27). Owner: not yet attributed (terrain split decision or unplaced vertices).

## Observed
Player report `20261023-181541-textures-close-to-camera-may-vibrate-when-walkin`, pad frame 5285, mark x 36-58%
y 79-94%: the path stones right in front of the camera shimmer while Spyro walks.

## Reproduction so far
`scratch/player-bugs/run_replay.py <report> vib 3000 PSXPORT_NOPACE=1 PSXPORT_PAD_SHOT_AT=5282..5286` reaches the same
path in the same level (`vib/compare.png`), with the camera slightly turned from the player's frame. The five
`PSXPORT_PAD_SHOT_AT` pictures are identical in the marked crop: a pad shot is the logic frame's record picture, so it
shows neither the in-betweens nor the field-to-field motion the player sees.

## Not yet known
Whether the shimmer is the terrain owner switching a near polygon's subdivision between frames, or near vertices
presented without their sub-pixel places. Telling them apart needs the presented sequence (in-betweens included) over
the walk, cropped to the mark, against the guest's own subdivision choice per polygon.
