# 0233 — Spyro 1: native depth reverses slot order where surfaces touch

Status: open (2026-11-04). Owner: psxport native depth (`docs/presentation.md` Depth,
`runtime/psx/render/record_rasterizer.cpp:RecordRasterizer::drawQuads`).

## Observed
Peace Keepers hub, `warp 1/0/0`, `1/0/1`, `1/0/2`, then `1/0/3`, 700 fields each (`scratch/depth3/pad.txt`), ires 8x,
16:9 4K sink, unpaced: with depth on, a grey rectangle of the fairy pad's buried skirt, with a step in its edge, draws
over the sand to the pad's front right. Depth off (slot order) shows the sand over it and the pad whole
(`scratch/depth3/w1-0-3-pad.png`: off, on, on without the terrain bias).

## Cause
Both are Spyro 1 terrain (`DepthOrder::Polygon`). The skirt lies just under the sand. Retail sorts the large sand
polygon by its depth sum, nearer than the skirt's, so the sand draws later and covers it. Per pixel, the skirt is within
one bin of the sand. `guest_terrain_polygon.h:sortedBack` moves each vertex back by its draw word's bin bias, and the
sand's bias is larger, so the skirt tests nearer. Without `sortedBack` the skirt is hidden (more than retail hides
it), but the Toasty start plinth and Dark Hollow portal logs lose the skirt retail draws over the grass. Neither rule
reproduces both, because both are contacts within one bin, below the resolution of the guest's own sort.

## Fix (not done)
A later slot draw loses only where it is farther than what is there by more than one bin (32 view units). The test
carries that margin and the written depth does not, so one fixed-function depth test cannot do it. The candidate: keep
true depth in a colour plane and test it in `record.frag`. That is exact only if overlapping opaque ops are in separate
batches, which the planner does not do now (`RecordPlanner::Builder::commit` splits only on reads). A stencil second pass per run does not do
it: triangles of one run would not test against each other. `sortedBack` stays: it is the guest's rule and keeps the plinths, which the
reviewers reported.
