# 0249 — Spyro 1's loading picture lasts its own fade and stage cadence

Status: open; the stages after the music gate run in one update, a press ends the music fade. Owners:
`titles/spyro1/core/frame/level_transition_update.cpp` (the stages), `titles/spyro1/core/frame/transition_skip.cpp`
(the press).

## Observed
With reads due at once, a level change still shows the loading picture (Spyro flying over the level's colour) for 19
logic frames, 38 fields. Route: `warp 0/0/0`, then `warp 0/1/0` from play, through `CheatProcessLevelWarp`:
`GS_LevelTransition` from field 2021 to gameplay at 2059 (`scratch/s1load/seq_sheet.png`, shots 1424-1442;
per-frame words in `scratch/s1load/w5.log`).

| logic frames | `g_LoadStage` | what holds it |
|---|---|---|
| 1423-1430 (8) | 3 | `LoadLevel`'s gate `(g_CdMusic.m_Flags & 0x40) == 0`: flags 0x200 for 7 frames, 0x100 for 1 |
| 1431-1441 (11) | 4-13 | one stage per call; 5 takes two calls (two VRAM halves) |

`g_CdState.m_IsReading` is clear on the frame after each read is issued, so no read waits.

## Cause
`LoadLevel` (loaders.c) runs once per logic frame from `func_8002DF9C`, the `GS_LevelTransition` update, and each
stage returns after it issues its read. Stage 0's `KillSoundsAndMusic(0)` starts the music fade in `spu.c`
(flags 0x200, the CD volume stepped down by an eighth per `CDMusicUpdate`); at silence it issues `CdlPause`
(0x100), and the next update sees the pause complete (0x40). Both are the guest's own timing, not drive time.

## What the picture waits on once the data is in
Each stage that issues a read returns, and the next call polls it at its gate (`CDLoadTime`, `m_IsReading`,
`CdSync`). The read is complete when it is issued (`cd_queue.cpp`), so stages 4-11 waited only for the next
update's call, one frame each: a drive poll, not loading work. The guest's own synchronous loads call `LoadLevel`
in a loop for the same reason (update.c `while (g_LoadStage < 10) LoadLevel(...)`).

## Without a press
`spyro1::registerLevelTransitionUpdate` owns the `GS_LevelTransition` update `func_8002DF9C` natively: Spyro's
flight, the camera and the tally as the guest calls them, then `LoadLevel(1)` under the guest's own tally gate
(`g_LoadStage < 11 || !g_LevelTransHudActive`), called again in the same update while its last call moved the
stage, or stage 5's VRAM half (`D_8007576C`), on (`spyro::drainLoader`). It stops before stage 12, which waits
for the camera to settle on Spyro on a portal entry, and wherever a call moves nothing: the music gate, the SPU
transfer, the tally. Every frame is drawn and presented; no field runs unshown and no word is written.

Measured 2026-11-13 (`scratch/loadwait/before`, `after`; `warp 0/0/0`, then `warp 0/1/0` at step 1200):
stage 3 holds 1423-1430 on the music gate as before, stages 4-12 run at 1431, 13 at 1432, gameplay at 1433
instead of 1442: the flight 10 frames instead of 19, change to gameplay 40 -> 22 fields
(`after/s1_sheet.png` against `scratch/s1load/seq_sheet.png`; the fade-in after it is unchanged).
The music fade (8 frames) is the drive's pause before data reads and stays on the press.

Through the portal (2026-11-13, `scratch/portal/`: `warp 0/0/0`, then `warp 0/1/0` at step 1700, which from
Artisans now enters Stone Hill's portal trigger region through `DispatchSpyroTriggerEvent`): stage 2 at 1790, 11 by
1800, held there under the tally until 2021; stage 12 for one update (the camera already on Spyro), 13 the next,
then GS 9 for 34 updates and gameplay, Spyro at (133162, 121859, 20983). A Cross at 1850 ends the tally
(`level-transition tally cancelled`), stage 12 and 13 follow at 1852-1853, same landing (`fine.log`, `tally.log`).

## Press skip
A Start/Cross press while the fade runs writes the CD volume (`0x80075F18`) to the fade's target, so the next
`CDMusicUpdate` (`0x8002BBE0`) ends the fade itself, issues `CdlPause` and frees the gate; the stages keep
their order. Measured 2026-11-12, before the stages ran in one update (`scratch/s1load/press_sheet.png`, `press.log`): tapped at
step 1424, taken at 1426, stage 4 at 1427 instead of 1431, gameplay at 1438 instead of 1442 (the picture 15
frames instead of 19). Paced (`paced_press.log`, `paced_nopress.log`): `warp` change to arrival 0.643 s ->
0.510 s, presents p50 16.75 ms in both. The press edge is latched per field by
`FieldOwner::takePresentationSkip`, since a logic frame spans two fields and the edge of the first was lost.
