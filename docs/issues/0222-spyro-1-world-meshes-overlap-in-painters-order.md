# 0222 — Spyro 1: world meshes overlap in painter's order at high resolution

Status: fixed (2026-11-04): native depth on by default; shown at the report's ledge corner from Spyro's own camera, not
the report's exact camera (below). Owner: psxport native depth
(`docs/presentation.md` Depth); Spyro 1 marks `terrain_image.cpp`, `actor_emitter_producer.cpp` and
`paired_actor_producer.cpp`.

## Observed
Player report `20261031-174745-depth-issues`, pad frame 15397, mark x 43-56% y 46-60%: a pale ledge-trim polygon
crosses Spyro's feet at a ledge corner in Stone Hill. The report's `psx.png` (device) shows the same overlap, so this is
the guest's own OT sort showing at 8x, not a record-vs-device divergence.

## Cause
Retail orders terrain by its polygon's depth sum (`guest_terrain_polygon.h:polygonBin`, sum >> 7) and a moby whole by
its centre (`actor_emitter.cpp:ActorEmitter::place`, view z >> 5; Spyro `paired_actor.cpp:PairedActor::placeCentre`,
z >> 7). A polygon that straddles the bucket of a nearer model is drawn over it where it is really behind.

## Fix
Native depth: the GTE place keeps each projection's view depth; Spyro 1's terrain, moby and Spyro producers are marked
opaque world 3D and depth-test against each other, everything else keeps slot order. The moby and Spyro draws name
their depth unit (`DepthUnit`), derived from each site's transform against the terrain's (`kNearViewShift`,
`kDepthBinShift` in `guest_render_globals.h`): a moby's is its position shift less the terrain's view shift, Spyro's
centre is world units (2^-kNearViewShift), the far pass is 2^(kFarViewShift-kNearViewShift). Each also takes the
guest's own sort bias (`sortedNearer_`, in bins) as a nearer offset, so what retail sorts in front stays in front.
Every guest store forgets the vertex depth of the words it writes (`Core::bookStore`), so no draw inherits one.

## Near-camera terrain
With depth on, the Stone Hill tower wall next to the camera lost its pixels to the grass below the terrace. Cause:
`game/render/terrain/guest_terrain_split.cpp:projectGrid` projects a grid point whose coordinates fit at 16x scale
(`loadGridVertex`) and shifts that SZ back by 4 for the guest's own depth code, but the vertex's place kept the
unshifted SZ, so the near fine and coarse grid carried 16x its depth. The scaled projection now opens a `DepthUnit` of
1/16. A replay of that frame's depth planes (`scratch/depth2/tools/planes.py`) showed the wall at 3000-4000 view units
over grass at 1500-2800 before, and the wall at 320-500 after.

## Proof
Stone Hill dragon 2 terrace, `warp 0/1/3`, 1600 fields, `spyro-at 196200 148000 27000`, 200 fields, ires 8x,
16:9 4K sink, unpaced (`scratch/depth2/stone-final.png`, off above on, then the difference): the wall is whole; the only
difference is the ledge block's corner behind the tower edge, whose triangles lie 25% farther than the wall they
overlap and which slot order drew over the tower's edge. Spyro and the trim at his feet draw the same. The arrival walk
(`scratch/depth2/artisans-a.png`, `artisans-b.png`, densest difference per arrival, off left, on right): a hill no
longer notches the hub castle tower (`dragon 1`), the Dark Hollow grass no longer steps into the water wall
(`dragon 3`), and buried plinth and log bases meet the grass along their contact line (Toasty start, Dark Hollow
portal); every other Artisans arrival differs in under 250 of 2,073,600 pixels.

The report's camera looks at Spyro from in front; `spyro-at` places him but the game's camera follows his heading,
which a yaw byte write does not change, so that exact view is not reached.

## Remaining
Lit and HUD mobys from the sprite queue (`sprite_queue_producer.cpp`) are not marked: that producer also draws the HUD
models.
