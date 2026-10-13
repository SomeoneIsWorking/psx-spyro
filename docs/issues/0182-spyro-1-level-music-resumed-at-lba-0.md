---
id: 182
title: Spyro 1 level music resumed at LBA 0 after every pause
status: resolved
symptom: after a level flyby is skipped the music never comes back; the title-screen music plays in fragments
tags: spyro1,audio,xa,cd
created: 2026-10-13
updated: 2026-10-13
---

## Observed
Player build and environment, `pick spyro1`, the user's pad replayed in a mapped window. The title music
(XA LBA 113448) decoded 97 of 238 sectors and played ~40% of the time. In the level the music driver paused
and resumed with `ReadS 00:02:00` (LBA 0); no audio sector was decoded for the rest of the run.

## Cause
Two framework defects (psxport issues 0153 and 0154) and one missing title fact: `CDMusicUpdate`
(`external/spyro-1/src/music.c`) tracks its position from `CdSync`'s GetlocP result only when
`CdLastCom()` reads GetlocP, and Spyro 1 declared no libcd work area, so the native command service never
wrote `CdLastCom()`'s byte at `0x80074E55` (written by the command send at `0x80064EA4`).

## Resolution
`titles/spyro1/core/runtime/spyro1_runtime.cpp` declares `stockCdWorkArea = {0x80074E50, 0x80074E54,
0x80074E55}`; psxport answers GetlocP from the drive head and publishes the last command (0154), serves
every elapsed drive sector (0153), and starts push-mode XA at pitch (0155).

After: the title stream decodes 238 of 238 sectors at -0.01% pitch; the level resumes at LBA 60000 and
60099 and plays continuously at +0.03 to +0.07%.

Spyro 2 and 3 do not declare their libcd work areas either; not measured.
