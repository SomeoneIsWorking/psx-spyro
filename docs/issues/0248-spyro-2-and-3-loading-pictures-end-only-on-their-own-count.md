# 0248 — Spyro 2 and 3 loading pictures, the new-game card among them, end only on their own count

Status: open; a press ends the picture's count. Owner: `game/core/loading_picture_skip.cpp`, facts in each
title's `core/boot_facts.h`.

## Observed
With reads due at once, entering a level still shows the loading picture: Spyro 2's `warp 0/1/0` fades the
title out over 932-948, shows the picture 952-972, black 980-988, then the level fades in 992-1000
(`scratch/loadfade/seq_sheet.png`). Spyro 3 shows its picture the same way.

## Cause
The level load is game state 5. Its sub-state 0 is the picture: Spyro 2's `0x80019364` loads it at counter
`0x800698EC` 0 and leaves sub-state `0x800698F0` 0 only when its ++ reaches 0xE (at 0xB it also finishes
an autosave when `0x80063278[kind]` is set); Spyro 3's `0x80058778` loads it at counter `0x8006FBC4` 0 and
leaves sub-state `0x8006FBC8` 0 only once it reads 0xD. Neither has another exit, and neither reads the pad.
The draws (`0x800151F0`, `0x80059038`) light the picture count * 16 until count 8 and fully after it;
Spyro 3 adds the caption past count 7. The later sub-states draw no picture (Spyro 2's 1 draws nothing,
Spyro 3's 1 and 4 only bars), so each display buffer keeps the last picture drawn into it through the load.

## The card before the new-game cutscene
The same state. Spyro 2's "In the world of Dragons" storybook is kind 1: state 5 from step 1313 to the first
cutscene frame (state 6) at 1363. Spyro 3's "An Evil Plot Unfolds..." caption is kind 6: sub-state 0
1213-1226, the load in sub-state 1 1227-1249, state 6 at 1269 (New Game from the title by Start taps,
`scratch/newgame/trace.py`).

## Without a press
Nothing changes; the count is the picture's own time.

## Press skip
`spyro::LoadingPictureSkip` latches a Start/Cross edge while sub-state 0 shows the picture (count 1 on; at
count 0 the previous screen is still fading out and a press there is dropped) and, before the next update, writes the count to the title's landing: 0xB, or one below it, so the update still draws two
full-brightness frames, one into each display buffer, and leaves through its own test. Spyro 2 lands at most
at 0xA while a pending autosave has not run, so the autosave at 0xB still runs. The landing keeps the
number of dropped updates even. Each update draws into the other buffer, and the load's last frame clears
the buffer it reaches with that draw env's background: Spyro 3's env `0x8006FBFC` carries the blue
`0x2D4FA2`, so an odd drop showed one blue field at the cutscene's start (`scratch/newgame/s3n`, `s3e`).
An earlier landing that drew only one full frame left the other buffer at the brightness the count had
reached, shown dim through the whole load (`scratch/newgame/s3k`, `s3b_sheet.png`).

Measured 2026-11-12 (each buffer's mean colour and the shown picture per step, `scratch/newgame/bufs.py`):

- Spyro 3 Sunny Villa (`warp 0/1/0`, `scratch/loadpress/s3vp1`, `s3vp2`, `s3vn`): count 4 -> 10 and 5 -> 11,
  both buffers full for the load, the picture's fade-out and the level fade-in unchanged, gameplay at f1087
  instead of f1093.
- Spyro 3 new-game card (`scratch/newgame/s3q`, `s3r`, no press `s3l`): count 3 -> 11 and 4 -> 10, both
  buffers full with the caption, the cutscene fades in from black, state 6 at 1261 or 1263 instead of 1269.
- Spyro 2 Glimmer (`scratch/loadpress/s2p1`, `s2p2`, `s2n`): count 7 -> 11 and 8 -> 10, both buffers full,
  gameplay at f1000 or f1002 instead of f1004.
- Spyro 2 new-game card (kind 1, no autosave; `scratch/newgame/s2l`, `s2m`, no press `s2j`): count 6 -> 10 and
  7 -> 11, both buffers full, state 6 at 1360 instead of 1364; a press during the title's fade-out at count 0
  is dropped and the card runs its own count (`s2k`).
- Paced at `ires=0`, `fps60=1`, 3840x2160 (`scratch/loadpress/paced_*`): `warp` start to gameplay Spyro 2
  2.324 s -> 2.225 s, Spyro 3 2.427 s -> 2.225 s, presents p50 16.75 ms with and without the press.
