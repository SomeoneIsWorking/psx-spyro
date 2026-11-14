# 0248 — Spyro 2 and 3 loading pictures, the new-game card among them, end only on their own count

Status: open; Spyro 2's and Spyro 3's level pictures end without a press and are never drawn, the story cards
end on a press. Owners: `game/core/counted_screen_skip.cpp` (the press and the loading-only end),
`game/core/boot_prefix_frame_driver.cpp` (the leaving update run before a draw), the screens and `kLevelPictures`
in each title's `core/boot_facts.h`, `titles/spyro3/core/loader_checksum.cpp` (Spyro 3's stages).

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

## What the picture waits on once the data is in
The count runs before the level load starts: sub-state 0 reads only the picture (at count 0) and, in Spyro 2,
finishes a pending autosave at 0xB. The load is sub-state 1.

- Spyro 2's sub-state 1 loads synchronously in one update (`while` loops over the stage step `0x8001D658`), so
  nothing waits once the data is in: state 5 entry to gameplay is 106 fields, of which the load is one update
  (`scratch/loadwait/before/s2.trace`, steps 932-1003). The rest is the picture's count, the two fade-out loops,
  state 3's hold and its fade-in, each on its own count (issue 0250).
- Spyro 3's sub-state 1 (`0x80058778`) calls the loader `0x8002CA50` once per update until its stage
  `0x8006C518` is -1. Each stage that issues a read returns, and the next call's poll `0x800503F8` waits for it;
  stage 4 (the overlay step `0x8007494C`) also steps `0x8006C5E4` once a call through two VRAM halves, eight SPU
  chunks and its last read. With reads due at once those calls waited for the next update, not for data:
  sub-state 1 took 23 updates, stage 4 eleven of them.

Spyro 3's loader override calls the loader again in the same update, only from the loading screen's own call
(jal `0x800588A0`), while its last call moved the stage or stage 4's step on (`spyro::drainLoader`). The other
callers (the portal flight, the skies, the cutscene loads) stop at their own stage thresholds and keep their one
call. Before each call it takes the interrupts owed since the last (`psx::cpu::takePendingInterrupts`): a read's
completion is due at once, but a running native refuses interrupts (`Hle::canDispatchInterrupt`), so without it
`CDLoadTime` (`0x800503F8`) found `isReading` still set and every read cost an update. A call that moves nothing
ends the update: the autosave stage 7 waits on, the checksum's end. Measured 2026-11-13 (`warp 0/1/0`, `scratch/loadwait/before`, `after`): sub-state 1 1033-1055 ->
1033-1041, gameplay at step 1079 instead of 1092, state 5 entry to gameplay 130 -> 104 fields; the picture stays
full through the load, then its fade-out and the level's fade-in as before (`after/s3_sheet.png`). With the owed
interrupts taken between calls (`scratch/loadwait3/after.log`): stage 1 at 1034, the six remaining reads all in
1035, gameplay at 1072 instead of 1079.

## Level pictures end without a press
A level picture only covers the load: in Spyro 3 a kind (`0x8006FBCC`) of 0..4, which sub-state 3 sends into a level
(5 on are story cards: New Game 6, the cutscene cards after it 14 and 15); in Spyro 2 a kind (`0x800698F4`) of 0, 3,
5, 9, 13 or 16, which sub-state 2 sends into the level through `FUN_800171BC` (the others start a cutscene, open the
guidebook or branch on the level; spyro-2 `docs/re-frontier.md` `spyro2.loading-picture`). Each title's
`kLevelPictures` names them.

The count-0 update runs as the guest makes it. What the load takes from it is its `CDLoadSync`'s wait: in Spyro 3
`while (CDLoadTime())` runs `CDMusicUpdate` until the XA music's stop (`0x8006E48C` 8, 9, 0, two `CdlPause`) is
over, before any loader read (spyro-3 `docs/re-frontier.md` `spyro3.loading-picture`). Skipped (the count put at 0xD
before it, `scratch/loadpic2/skip2`, `skip4`), the stop runs inside the loader's own polls, and the 18-sector read
stage 6's step issues just after a `CdlPause` (`[cdc] cmd 0x09`, issued from libcd's `0x800614C4`) has its
completion announced behind that pause's responses (`queue_empty=false`, `I_STAT=0x004`); the guest's handlers
clear the CD bit (`w I_STAT 0x7FB` at `0x8005C848`, `0x8005C984`), no further CD edge is raised, the data-ready is
never delivered (`19 of 19 delivered`, no 20th; `psx::cd::readyCompletionOwed` needs that bit), and stage 7 waits 75
updates for `CDLoadTime`'s own timeout to re-issue the read. That lost completion is a framework CD defect, not
traced further here; the picture keeps its count-0 update, so the load never meets it.

After that update `CountedScreenSkip::leavesBeforeDraw` tells the frame driver the picture is a level picture, and
the driver runs the next update at once instead of the draw: before it the count goes to the screen's end (0xD),
or in Spyro 2 to 0xA when `0x80063278[kind]` asks for the autosave (kinds 5, 9, 13, 16), so the 0xB update still
runs it and the following one leaves. The picture is never drawn: the frame before it, the guest's count-0 draw
(Spyro 3 `func_8001EBAC`, both buffers cleared black; Spyro 2 the screen before faded out by `0x8004D810`), holds
until the load's own frames. The fade-out loop after the load (Spyro 3 `0x80020168` at state 5 sub-state 3, Spyro 2
`0x8004D810` at sub-state 2) keeps its last two passes for the same kinds; Spyro 2's run of the same loop at
sub-state 0 on the screen before is not a level picture's and keeps its count.

Measured 2026-11-14, unpaced, hidden, from the warp's start (`scratch/loadpic2/`):

- Spyro 3 `warp 0/1/0`, Sunny Villa from the title (`s3final.trace`, `s3final_sheet.png`; before f17e5bde
  `scratch/loadpic/s3.trace`, f17e5bde `s3after.trace`, `s3after_sheet.png`): sub-state 1 at field 1019 instead of
  1021, gameplay at 1046 instead of 1048 (1072 before f17e5bde). Fields 1017-1031 are black but for the guest's own
  `(8, 8, 8)` draw-environment clears under the load's bars (1019-1023, mean 1-7); the 1/8 picture at 1020-1025 is
  gone. Landing identical: Spyro (0xF8A4, 0x533D, 0x2F67), camera (0xF8A4, 0x498C, 0x31D4), level 0xB, as
  `scratch/loadpic/s3after_pos.trace`.
- Spyro 2 `warp 0/1/0`, Glimmer from the title (`s2head.trace` HEAD, `s2after.trace`, `s2after_sheet.png`): the
  title fades out over 932-948, then black until the level fades in; sub-state 1 at 949 instead of 962, state 3 at
  956 instead of 981, gameplay at 977 instead of 1002: 25 fields. Landing identical: Spyro (0x10C66, 0x818F, 0x2667),
  camera (0x1140A, 0x7B97, 0x28D4), level 0xB, and the same camera move 15 fields after arrival.
- Spyro 2 autosave landing (`s2save.trace`, kind 0 given the autosave flag by a REPL write for the test): count
  1 -> 0xA, the 0xB update runs the autosave (`0x80067E04` 0 -> -1, no card), 0xB -> 0xD, sub-state 1 at 949, all
  in one frame; gameplay at 977, same landing. A real saving kind needs the title overlay resident (the autosave
  is in it); entering state 5 by a REPL call from a level jumps to pc 0 at 0xB with or without this rule (`s2k5`).

## Spyro 3's load: what is left is guest work
The owed read completions are taken by the loader drain before each call (`psx::cpu::takePendingInterrupts`): every
read in the load is issued and completed in the same update (`scratch/loadpic/s3irq.log`, 22 of 22 delivered). The
load still spans two updates because stage 0's `ResetHuds` (`0x800282D8`) walks the sprite table at `[0x8006C738]`
(`0x800A2C7C` from the title) in `GetSpriteIndex` `0x80027934` for 1.96M cycles, 3.5 fields of guest CPU: the call
leaves the field's budget, the update finishes on later turns without the drain, and stages 1-12 run on the next
update. That is the guest's own cost, not a drive wait.

## The card before the new-game cutscene
The same state. Spyro 2's "In the world of Dragons" storybook is kind 1: state 5 from step 1313 to the first
cutscene frame (state 6) at 1363. Spyro 3's "An Evil Plot Unfolds..." caption is kind 6: sub-state 0
1213-1226, the load in sub-state 1 1227-1249, state 6 at 1269 (New Game from the title by Start taps,
`scratch/newgame/trace.py`).

## Without a press
The story cards (Spyro 2's cutscene and guidebook kinds, Spyro 3's kinds from 5) run their own count; level pictures
end as above.

## Press skip
`spyro::CountedScreenSkip` latches a Start/Cross edge while sub-state 0 shows the picture (count 1 on; at
count 0 the previous screen is still fading out and a press there is dropped) and, before the next update, writes the count to 0xB, or one below it (`spyro::keptUpdates`, `game/core/even_skip.h`), so the update still draws two
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
