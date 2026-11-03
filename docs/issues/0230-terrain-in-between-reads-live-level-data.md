# 0230 — A terrain in-between reads level data the game has since overwritten

Status: open (2026-11-04). Owner: `game/render/terrain/terrain_rebuild.*` (`Rebuild::emit`, its read overlay).

## Observed
Spyro 2, from play with a camera turn, a level change through the storybook load `0x80019230(0)` while preseq holds
in-betweens pending aborts in `Rebuild::emit`: `[packet-chain:error] packet 0x8AB94EB8 ... not a polygon`
(`scratch/spyro23/repro2`..`repro5`, `walk2b` logs). A rebuild re-runs the terrain drawer over the saved reads of
its frame, but about 35 words per rebuild are not in the saved set and come from live guest memory (79 at the
abort); the storybook load has replaced them with the next level's data by the time the in-between composes.

A hidden unpaced run composes in-betweens only when preseq forces them; the player's window always composes them,
so a load that overwrites level data inside one present interval is a candidate for the same abort in play.

## Next
Find which drawer reads miss the saved set (the reads overlay in `terrain_state_producer.cpp`) and record them with
the frame, so a rebuild never reads live memory. A first try that overlaid the previous frame's reads did not cover
them and was reverted. The warp no longer takes this load (issue 0227);
the game's own `0x80019230(0)` from play is the credits cheat's return (`0x8001A9B0`), which reproduces it with a turn
and preseq pending.
