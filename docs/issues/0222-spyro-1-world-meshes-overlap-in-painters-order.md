# 0222 — Spyro 1: world meshes overlap in painter's order at high resolution

Status: open (2026-11-04): native depth is on by default and every opaque world producer is marked, but the report's
stance (Spyro on the step by the tower) is not reached, so its overlap is not judged on our build (below). Owner: psxport native depth
(`docs/presentation.md` Depth); Spyro 1 marks `terrain_image.cpp`, `actor_emitter_producer.cpp` and
`paired_actor_producer.cpp` and `sprite_queue_producer.cpp`.

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
(`dragon 3`). Buried plinth and log bases first lost their skirt to the grass (Toasty start, Dark Hollow portal);
a terrain polygon now leaves its depth moved back by one bin and the guest's bin bias, and tests at its true depth,
so the skirts draw as with depth off (issue 0233).

## At the report's camera
`spyro-at x y z heading` writes Spyro's heading (`teleport.cpp`, +0xCC and +0x11C, 12 bits a turn). Placed at heading
1400, settled 200 fields, then turned to face the camera for 25 fields (`scratch/depth3/stone.txt`, `block2.txt`): the
view matches the report's, with the tower at left and the pale-trimmed step beside Spyro. Spyro lands on the floor
(z about 25530) at every placement; a placement over the step's footprint pushes him out to the floor, so the report's
stance on the step is not reached. At six floor placements beside the step, depth off and on draw his feet and the
trim the same (`scratch/depth3/r0222-feet.png`, `blk2-sheet.png`: off left, on right).

## Sprite queue
The sprite queue (`titles/spyro1/render/sprite_queue.cpp`, `0x80022A2C`) draws world and HUD records. A world record
now projects through a `DepthUnit` whose scale is its centre's view z over the doubled centre it is translated by, less
the bins its bias sorts it nearer; a HUD record keeps scale 0, so its vertices have no depth and keep slot order. The
producer is marked `Object`. `PSXPORT_DEBUG=spritequeue` logs each record's scale and bias: at Stone Hill, Dark Hollow,
Toasty and Peace Keepers 1/1/0, world records draw at scale 0.5 and 4 bins nearer, HUD records at 0
(`scratch/depth3/runs/sq/run.log`). Every Artisans and Peace Keepers arrival draws the same as before the queue was marked
(0 differing pixels against `art-on` and `pk-on`).

## Remaining
The report's own stance (Spyro on the step) is not reached through `spyro-at`; the overlap it shows is retail's sort,
which native depth corrects for Spyro's draw (`Object`, true depth). A tree base over a grass hill at Stone Hill is
issue 0233.
