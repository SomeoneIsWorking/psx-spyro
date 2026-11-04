# 0234 — Spyro 3's new-game cutscene switches between 16:9 and a 4:3 pillarbox shot by shot

Status: open (found 2026-11-04, spyro23c boot walk).
Owner: not traced yet.

## Observed
From boot at 16:9 (judgement settings, hidden, unpaced), Start then New Game from the title, the opening cutscene
("An Evil Plot Unfolds...", "A Powerful Villain Emerges...", "A Desperate Rescue Begins...") draws some shots
across the whole 16:9 canvas and others only in the 4:3 centre with black margins. In the walk (a shot every 100
steps from title step 1600): pillarboxed at steps 2500-2800, 3300-3600, 4000-4400; full width at 3000-3200,
3800-3900, 4600. Every pillarboxed sample carries a caption; the full-width ones do not. The picture jumps between
the two framings at shot changes.

Evidence: `scratch/spyro23c/walk3/cutscene.png` (steps 2500, 3100, 3400, 3900), `walk3/sheet.png` (the whole walk),
`scratch/spyro23c/tools/walk.py`.

## Next
Reproduce on one pillarboxed shot (dump `scene` and the GTE `H`/OFX on a caption step and a full-width step) and
find whether the caption shots use another projection or clip owner than the widened 3D view.
