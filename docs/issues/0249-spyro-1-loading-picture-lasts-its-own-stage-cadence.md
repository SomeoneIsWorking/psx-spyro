# 0249 — Spyro 1's loading picture lasts its own fade and stage cadence

Status: open; a press ends the music fade. Owner: `titles/spyro1/core/frame/transition_skip.cpp`.

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

## Without a press
Nothing changes: ending the fade early writes the fade words, and running more than one stage per frame runs
`LoadLevel` faster than the guest does; G005 rules out both.

## Press skip
A Start/Cross press while the fade runs writes the CD volume (`0x80075F18`) to the fade's target, so the next
`CDMusicUpdate` (`0x8002BBE0`) ends the fade itself, issues `CdlPause` and frees the gate; the stages keep
their one-per-frame cadence. Measured 2026-11-12 (`scratch/s1load/press_sheet.png`, `press.log`): tapped at
step 1424, taken at 1426, stage 4 at 1427 instead of 1431, gameplay at 1438 instead of 1442 (the picture 15
frames instead of 19). Paced (`paced_press.log`, `paced_nopress.log`): `warp` change to arrival 0.643 s ->
0.510 s, presents p50 16.75 ms in both. The press edge is latched per field by
`FieldOwner::takePresentationSkip`, since a logic frame spans two fields and the edge of the first was lost.
