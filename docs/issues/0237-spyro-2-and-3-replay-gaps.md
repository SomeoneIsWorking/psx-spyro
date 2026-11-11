# 0237 — Spyro 2 and 3: what the draw body's replay does not move yet

Status: open. Owner: `game/core/draw_section.*` (the replayed states), `game/core/draw_motion.*` (the tracks),
`titles/spyro2|3/core/draw_section.*` (each image's switch); plan `docs/plans/spyro23-replay.md`.

## Observed
Since field_redraw and the S2/S3 producers were deleted (2026-11-10), every Spyro 2/3 in-between is a replay of the
draw body 0x800156FC / 0x8001E638. Gameplay moves halfway (Glimmer `warp 0/1/0`, Sunrise Spring `warp 0/0/0`,
`scratch/replay23/s2c`, `s3c`). These do not:

1. **Loading screen, state 5** (S2 0x800151F0, S3 0x80059038): never replayed, held at 30 Hz. S2's handler polls
   until it runs out of budget at 0x800577A0; S3's waits in VSync at 0x8005927C (`scratch/replay23/s3a.log`, 216
   refusals before `declinesState` named the state; each declined frame is reported at run end by reason). A state the switch has no case for (S3 8, 0x11) is not
   replayed either.
2. **Inputs that still step at 30 Hz**:
   - Spyro 3's HUD slides (plan Open 8): its widgets keep the slide in byte +0x3F and read the table `0x800674E8`
     inline in `0x80029708` and per digit in `0x800291B8` / `0x800293C4`; shown stepping in its pause
     (`scratch/spyro23-tracks/pause3_hud.png`: each in-between equals the next real). Spyro 2's are fixed (below).
   - Sky object wave (plan Open 7): the draw reads the shared sine table at `tick * 4 + object * 0x40` into
     `0x80068C08`/`0C0A` for `0x800296D0`; the table cannot be rewritten (other drawers of the section read it) and
     the consumer would need the object index from `0x8004C66C`'s frame. The objects are level data and do not move.
   - The flame breath's part timers (`0x8006AA3C..`) step; its anchor is tracked.
3. **Not shown since the change**: cutscenes (S2 6, S3 0x12) and the scene states: no warp reaches them yet. Spyro 2's
   tint and reflection shift (`SpyroSurface`) stayed 0 on every route tried; Spyro 3's letterbox and water were not
   reached (Spyro 3 has no `moby-teleport`).

## Fixed (2026-11-11, shown in frames, `scratch/hudslide`)
- Spyro 2's HUD slide: `HudSlide` (`game/core/hud_slide.*`) is `0x80052498`'s override and blends each call's
  offset inside a replay. Glimmer, Select: the gem icon moves down at every present, each in-between on the nearest
  whole native row to halfway (`s2_gem.png`); 343 of 343 calls equal the guest's own result outside a replay.
- Letterbox: inside a replay `screen_overlays::drawLetterbox` places the bars' moving corners between the row the
  body read and the row it drew (`rowPerDraw`). `moby-teleport 554 2`: both bars grow half a native row per present
  from 12 to 24 (`letterbox.png`). A bar shrinking to 0 is not drawn by S, so its last half row is not shown.
- Water: Summer Forest `start`, Up into the stream: Spyro's water pass is drawn in every in-between, his strokes and
  Sparx between their neighbours (`sfwater_seq.png`). Crush's Dungeon: Crush and the camera move halfway
  (`boss_seq.png`).
- Leak proof: Select, `moby-teleport 554 2`, Up and Left; fps60 on (906 in-betweens) and off: `fielddigest`
  identical over 2888 fields; Spyro 3 Sunrise Spring, Up and Left (746 in-betweens), identical over 2813.

## Fixed (2026-11-10, shown in frames, `scratch/spyro23-tracks`)
- Fades: the dispatcher's fade words are motion tracks (S2 `0x80066F68`, `0x80067150`; S3 `0x8006C598`,
  `0x8006C7C0`, red flash `0x8006C6D4` as a countdown, so its rise is never halved). The Glimmer arrival fade-in alternates t=0.5 / t=1 and each in-between's centre
  brightness lies between its neighbours (6.24 / 9.35 / 13.30, 13.30 / 17.41 / 21.95, 21.95 / 27.20 / 33.32).
- Spyro 2's conversation box: `ConversationSlide` writes the slide table entry state 1's handler reads (mode
  `0x80067134`, timer `0x80067130`). `warp 0/1/0`, `moby-teleport 554 2`: the box's right edge moves 128 of 5472 columns
  per present through the slide and in equal steps through its bounce (`conv2_box.png`).
- Spyro 3's level-load refusal: `0x8001DC5C` in steps 0 and 4 of `0x8006FBC8` with the fade at 0xFF calls `0x8001EBAC`
  (DrawSync, whose GPUSTAT poll at `0x8005C224` never ends in the sandbox, VSync, ClearImage). That frame is no longer
  replayed (`spyro3::DrawSection::declinesState`); the screen is black there. The warp now runs with 0 refusals.
- Title screen (S2): composed in-betweens, Spyro's head turn halfway (`title2_spyro.png`).
- Pause and Sparx's second pass (S3): every in-between composed, Sparx drawn, his wings between their neighbours
  (`pause3_sparx.png`).
- Leak proof: same inputs, fps60 on (S2 920 in-betweens, S3 746) and off (0): `fielddigest` byte-identical over 2914
  and 2813 fields.
- Pacing (host load 2.4-2.7): a paced Glimmer and Sunrise Spring run each show 698 of 701 simulated 60.00 Hz vblanks
  advancing 0.5 tick, 3 repeats (the 59.82 Hz field rate).
