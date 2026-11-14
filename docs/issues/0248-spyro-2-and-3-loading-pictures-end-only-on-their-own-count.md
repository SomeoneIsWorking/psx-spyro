# 0248 — Spyro 2 and 3 loading pictures, the new-game card among them, end only on their own count

Status: open; Spyro 3's level pictures end without a press, its story cards and Spyro 2's pictures on a
press. Owners: `game/core/counted_screen_skip.cpp` (the press and the loading-only end), the screens in each
title's `core/boot_facts.h` (`kCountedScreens`, `kCountedLoops`), `titles/spyro3/core/loader_checksum.cpp`
(Spyro 3's stages).

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

## Spyro 3's level pictures end without a press
A picture below kind 5 (`0x8006FBCC`, the index `UpdateLoadingImage` reads it by; sub-state 3 enters a level only for
0..4, 5 on are story cards: New Game is 6, the cutscene cards after it 14 and 15) only covers the load. Its count-0
update still runs (the picture's read and LoadImage; without it loader stage 7 waits for good, cause not traced,
spyro-3 `docs/re-frontier.md` `spyro3.loading-picture`); before the next update the count is put at 0xD, so
sub-state 0's own test leaves for the load. The fade-out loop `0x80020168`, called only from that screen's draw,
keeps its last two passes for the same kinds. Story cards keep their count and their press.

Measured 2026-11-14 (`warp 0/1/0`, Sunny Villa from the title, unpaced, hidden; `scratch/loadpic/s3.trace` HEAD,
`s3after.trace`, `s3after_sheet.png`), fields from the warp's start at 1015 to the first gameplay field:

| fields | HEAD | now | what |
|---|---|---|---|
| title's start | 1015-1017 | 1015-1017 | the title's own exit |
| picture count | 1018-1032 | 1018-1020 | presentation, loading-only |
| load (sub-state 1) | 1033-1035 | 1021-1023 | stage 0's walk, then stages 1-12 in one update |
| fade-out loop (sub-state 3) | 1036-1053 | 1024-1029 | loading-only; 2 passes kept |
| level init (state 3 sub-states 2, 3) | 1054-1055 | 1030-1031 | the game's own steps |
| level fade-in | 1056-1071 | 1032-1047 | transition, ends on a press (issue 0250) |
| gameplay | 1072 | 1048 | |

Landing identical: Spyro (0xF8A4, 0x533D, 0x2F67), camera (0xF8A4, 0x498C, 0x31D4), level 0xB in both
(`s3head_pos.trace`, `s3after_pos.trace`). New Game (`scratch/loadpic/ng/ng2.out`): the kind 6, 14 and 15 cards each
run sub-state 0 for 15 fields, no loading-only end logged.

Left: the count-0 update increments the count before its draw, which lights the picture at 1/8 (count * 16), and
that frame stays on screen through the load's first update (fields 1020-1025, mean 7-9 against the title's 76).
Removing it needs what stage 7 takes from the count-0 update.

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
Spyro 2's pictures and Spyro 3's story cards run their own count; Spyro 3's level pictures end as above.

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
