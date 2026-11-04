# 0233 — Spyro 1: native depth reverses slot order where surfaces touch

Status: fixed (2026-11-04) for terrain against terrain; an object over nearer terrain is open (Remaining). Owner:
psxport native depth (`docs/presentation.md` Depth, `runtime/psx/gpu/record_raster_setup.cpp:RecordPlanner::Builder::depthGroup`,
`runtime/psx/render/record_rasterizer.cpp:RecordRasterizer::drawQuads`).

## Observed
Peace Keepers hub, `warp 1/0/0`, `1/0/1`, `1/0/2`, then `1/0/3`, 700 fields each (`scratch/depth3/pad.txt`), ires 8x,
16:9 4K sink, unpaced: with depth on, a grey rectangle of the fairy pad's buried skirt, with a step in its edge, drew
over the sand to the pad's front right. Depth off (slot order) shows the sand over it and the pad whole
(`scratch/depth3/w1-0-3-pad.png`: off, on, on without the terrain bias).

## Cause
Both are Spyro 1 terrain (`DepthOrder::Polygon`). The skirt lies just under the sand. Retail sorts the large sand
polygon by its depth sum, nearer than the skirt's, so the sand draws later and covers it. Per pixel the skirt is within
one bin of the sand. `game/render/terrain/guest_terrain_polygon.h:sortedBack` moved each vertex's depth back by its draw
word's bin bias, and the sand's bias is larger, so the sand tested farther than the skirt and lost. Without the bias the
skirt is hidden, but the Toasty start plinth lost the skirt retail draws over the grass. One depth for both the test
and what a polygon leaves cannot reproduce both: both are contacts within one bin, below the guest's sort resolution.

## Fix
A later terrain polygon loses only where its true depth lies farther than what is there by more than one bin plus the
bias it was sorted back by; the margin is in the depth a polygon leaves, never in its test.
- `guest_terrain_polygon.h:sortedBack` sets `GtePlace.back` (the bias in view units) and leaves z true. `back` travels
  with the vertex depth (`VertexDepths`, `EmitMemory`, the OT walk's `Gp0Origin.depth`, `RecordVertex.back`).
- Spyro 1 marks its terrain with a bin of 32 view units (`terrain_image.cpp`, `guest_render_globals.h:kDepthBinViewUnits`);
  the planner stores bin plus `back` in the `DepthPlane` (`kDepthBin`).
- The planner splits tested polygons into depth groups whose ops cover no native pixel twice; each group draws twice:
  test without depth write (colour and stencil where it passes), then the moved-back depth where stencil is set
  (`record.frag` variant `RECORD_DEPTH_BACK`, D24S8 target).

## Proof
Build `build/depth3`, ires 8x, 16:9 4K sink, unpaced, depth off against on:
- `padplinth.txt` (Toasty start `warp 0/4/0`, then the pad arrival `1/0/3`): the pad and the plinth match depth off; the
  12 and 15 differing pixels left are single edge pixels (`scratch/depth3/pp-cl-w0-4-0.png`, `pp-cl-w1-0-3.png`).
- Every Artisans and Peace Keepers arrival (`walk-art.txt`, `walk-pk.txt`, runs `art-off`/`art-on`, `pk-off`/`pk-on`;
  densest crops off left, on right: `art-crops-a.png`, `art-crops-b.png`, `pk-crops.png`, `pk-cl-w1-2-2.png`,
  `pk-cl-w1-1-4.png`). Largest: Artisans `0/0/1` 473 px, `0/1/3` 350, `0/2/4` 331 (was 777, 940, 1231); Peace Keepers
  `1/2/2` 1057, `1/1/4` 210, `1/2/1` 176. Each is depth correcting a visible slot-order error: the castle tower notch, a
  hill into the water wall, a jagged notch in a Peace Keepers rock pillar edge, a far ledge cut by a vertical edge in
  the 16:9 margin. No object or surface loses pixels slot order showed correctly.
- Stone Hill `0/1/4`: the wall-base sliver at the rock pillar's edge with depth on is gone (`stone14-pair.png`); it had
  the same cause (the trim tested farther by its bias).

## Remaining
Stone Hill `0/1/3` (`block2.txt`, fourth placement, `blk2-b4-mid.png`, off left, on right): a far tree's trunk base shows
over the grass hill in front of it (155 px at 8x). The tree is drawn over nearer terrain (it is not the sprite queue:
unmarking that producer leaves the picture equal); slot order then hides it with a later, farther grass polygon, which
with depth loses to the nearer terrain's depth. Marking mobys with the one-bin margin does not change it. Next: trace
which producer draws the tree and the grass polygon that retail lets cover its base.
