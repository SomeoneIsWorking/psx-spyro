# 0255 — Spyro 3's intro castle breaks apart in the right 16:9 margin

Status: closed (2026-11-15): not a defect. The margin towers are the game's own far LOD for distant sectors,
which retail draws the same way wherever its view reaches them (user verdict on the full-size shot). Owner: the S2/S3 terrain passes (`game/render/terrain/guest_terrain_classify.cpp` lists the
margin sectors; `guest_terrain_far.cpp` and `guest_terrain_detail.cpp` draw them).

## Observed
User report on `docs/media/spyro3-interpolation.webp`: the castle at the right edge is broken in both the 30 and
60 fps panels. Reproduced from boot: Spyro 3 idle boot, `aspect=1`, `ires=0`, `fps60=1`, sink 3840x2160, unpaced,
present 960 (the castle intro runs from present 720 to 1020). At 4:3 the castle is the textured model with
windows and an arch. In the right margin at 16:9 the towers are the flat-shaded far model: untextured orange
roof tiers floating apart, a tower body that stops flat, and a lone spike at the edge with nothing under it.
Evidence: `scratch/castle3/present960-4x3-over-16x9.png` (4:3 above 16:9).

## Why it is not a widescreen defect
The far pass draws a sector's low-detail mesh beyond its near limit, flat-shaded and untextured. The
castle's right towers sit past the detail depth from this fixed intro camera, so the widened view shows them
as retail shows any distant sector. The 4:3 centre's textured castle is nearer and takes the detail list.
