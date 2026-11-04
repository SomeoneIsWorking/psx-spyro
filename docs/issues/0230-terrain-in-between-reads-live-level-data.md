# 0230 — A terrain in-between reads level data the game has since overwritten

Status: fixed (2026-11-04). Owner: `game/render/terrain/terrain_state_producer.*` (`TerrainStateProducer::composition`)
and `terrain_rebuild.*` (`Rebuild`, its read overlay).

## Observed
Spyro 2, from play with a camera turn, a level change through the storybook load `0x80019230(0)` while preseq held
in-betweens pending aborted in `Rebuild::emit`: `[packet-chain:error] packet 0x8AB94EB8 ... not a polygon`
(`scratch/spyro23/repro2`..`repro5`, `walk2b` logs, on the build of 2026-11-03).

## Cause
`TerrainStateProducer::render` re-ran the drawer over the later field's saved reads only. At the in-between camera
the drawer reads words neither real field read: the draw, colour and texture words of polygons both fields culled
(backface, outcode, fade depth), and every word of a sector only the earlier field saw. Measured on Zephyr with an
L1 turn: 3 to 58 such words in about one rebuild in twelve (`scratch/spyro23b/r1/run.log`, `diag0230` lines). They
fell through to live guest memory, which by the time a present composes may already hold the next level's data.

## Fix
- Each field saves its reads in address order (`drawField`), and the rebuild overlays both fields' reads, the later
  field's value winning (`FieldInputs::earlierReads`, `laterReads`).
- The rebuild notes every word it reads outside its own ranges. A word neither field saved is trusted only when the
  saved words on both sides of it still hold their saved values live, so it is level data no load has replaced
  (`Rebuild::covered`).
- The producer runs the rebuild in `composition` and refuses the interval when it is not covered, so the frame is
  shown whole instead of mixing two levels; `render` emits that same rebuild.

## Shown
- Warp route (the old `0x80019230(0)` from play, stream step cleared, level 44 set; `scratch/spyro23b/tools/load0230.py`,
  Zephyr with L1 held, 16 presents dumped across the load): arrives in Fracture Hills with no error and no held
  interval. Requiring every read to be saved held 69 of 810 in-betweens during the turn; with the unchanged-neighbour
  rule all 810 compose (`scratch/spyro23b/r9/load-from-play.png`: turning play, then the storybook, no stray polygons).
- Credits cheat (pause, then the cheat at `0x80064F14` entry 1: square circle x3, left right x3), credits to their
  return `0x8001A9B0` → `0x80019230(0)`, presents dumped across the return (`credits0230.py`): the credits fade to black
  before the load, so no terrain in-between is pending at it; the return arrives in Zephyr cleanly
  (`scratch/spyro23b/c4/credits-return.png`).
- The abort itself did not reproduce on current HEAD through either route (three preseq timings tried on HEAD's
  terrain code, `r7head`, `r8head-*`): a cut record now composes from itself, so the load frame's own in-between
  reads only its own saved words. The live reads it relied on were reproduced and are gone.
