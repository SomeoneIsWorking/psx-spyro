# 0248 — Spyro 2 and 3 loading pictures, the new-game card among them, end only on their own count

Status: open, unskippable. Owner: none (no faithful terminal transition).

## Observed
With reads due at once, entering a level still shows the loading picture: Spyro 2's `warp 0/1/0` fades the
title out over 932-948, shows the picture 952-972, black 980-988, then the level fades in 992-1000
(`scratch/loadfade/seq_sheet.png`). Spyro 3 shows its picture the same way.

## Cause
The level load is game state 5. Its sub-state 0 is the picture: Spyro 2's `0x80019364` loads it at counter
`0x800698EC` 0 and leaves sub-state `0x800698F0` 0 only when the counter reaches 0xE (at 0xB it also finishes
an autosave when `0x80063278[kind]` is set); Spyro 3's `0x80058778` loads it at counter `0x8006FBC4` 0 and
leaves sub-state `0x8006FBC8` 0 only at 0xD. Neither has another exit, and neither reads the pad. The count
is the picture's own time, not a load wait, so the only ways to end it early are writing the counter or the
sub-state, which G005 rules out.

## The card before the new-game cutscene
The same state, so the same gap. Spyro 2's "In the world of Dragons" storybook is kind 1: state 5 from step 1313
to the first cutscene frame (state 6) at 1363. Spyro 3's "An Evil Plot Unfolds..." caption is kind 6: sub-state 0
1211-1226, the load in sub-state 1 with the picture still drawn 1226-1249, state 6 at 1268
(`scratch/newgame/s2c_sheet.png`, `s3a_sheet.png`; New Game from the title by Start taps, `scratch/newgame/trace.py`).
A press that brought the count to its exit was tried and reverted. The update did leave through its own step,
and the cutscene came 12 (Spyro 2) or 10 (Spyro 3) steps sooner. But Spyro 3's picture then stayed at the
brightness the count had reached, with no caption, through the whole load (`scratch/newgame/s3b_sheet.png`).
It is also a written timer word.

## Proper fix
None recovered. A faithful skip needs a guest path that leaves sub-state 0 without its count; none exists in
either update.
