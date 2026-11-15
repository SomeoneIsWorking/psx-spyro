# 0255 — Spyro 3's intro castle breaks apart in the right 16:9 margin

Status: open. Owner: the S2/S3 terrain passes (`game/render/terrain/guest_terrain_classify.cpp` lists the
margin sectors; `guest_terrain_far.cpp` and `guest_terrain_detail.cpp` draw them).

## Observed
User report on `docs/media/spyro3-interpolation.webp`: the castle at the right edge is broken in both the 30 and
60 fps panels. Reproduced from boot: Spyro 3 idle boot, `aspect=1`, `ires=0`, `fps60=1`, sink 3840x2160, unpaced,
present 960 (the castle intro runs from present 720 to 1020). At 4:3 the castle is the textured model with
windows and an arch. In the right margin at 16:9 the towers are the flat-shaded far model: untextured orange
roof tiers floating apart, a tower body that stops flat, and a lone spike at the edge with nothing under it.
Evidence: `scratch/castle3/present960-4x3-over-16x9.png` (4:3 above 16:9).

## Narrowed
The far pass draws only the vertices beyond a sector's near limit; the detail pass draws the rest. Far pieces
present with their bodies missing means the detail pieces of these margin sectors are not drawn.
- The classifier lists a margin sector by depth into the detail and far lists exactly as retail does. It leaves
  the sector's visibility byte zero and skips `animateSector` for it (issue 124's rule for the moby walk).
- The detail path's screen tests use `frame_.bounds`, the widened window, so they do not reject at column 512.
Not yet read: whether the detail drawer, or the sector's animation that `animateSector` would have run, is
what leaves a margin sector's detail polygons unbuilt.

## Next
Read the detail list consumer for its use of the visibility byte and the animation marks, and count margin
sectors' detail and far primitives at present 960.
