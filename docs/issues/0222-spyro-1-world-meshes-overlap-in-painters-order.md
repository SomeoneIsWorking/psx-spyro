# 0222 — Spyro 1: world meshes overlap in painter's order at high resolution

Status: open (2026-11-03): native depth landed, the report's own screen not yet shown. Owner: psxport native depth
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
their depth unit (`DepthUnit`): moby translation is view z * 4 >> position shift (* scale / 32), Spyro's is world units
(4x the terrain's, from the two bin shifts). Without the unit Spyro (z 2000-2700 against the floor's 500-900 at the
same pixels) vanished behind the terrain.

## Proof so far
`scratch/depth/runs/toasty-on` vs `toasty-off` (Artisans, `gate-teleport 0 0`, ires 8x, 16:9, depth on vs
`PSXPORT_DEPTH=0`): Spyro, Sparx's glow and his shadow are the same, and the rock wall meets the floor along its real
intersection instead of its slot-order overlap (`toasty-on/left_crop.png`). Stone Hill is reached with `warp 11`
(`stone-on/grid.png`), but the ledge corner needs Spyro placed there; the report replay does not reach it (issue 0225).

## Remaining
Show the report's ledge corner with depth on; add a Stone Hill placement to the warp verbs. Lit and HUD mobys from
the sprite queue (`sprite_queue_producer.cpp`) are not marked: that producer also draws the HUD models.
