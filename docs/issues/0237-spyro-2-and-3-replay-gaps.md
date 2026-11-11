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
   - The flame breath's part timers (`0x8006AA3C..`) step; its anchor is tracked.
3. **Not shown since the change** (tried 2026-11-11, `scratch/fade2`):
   - Spyro 2's tint (+0x20) is written by `0x80031EAC` from the power-up timers `0x8006A1C4`, `0x8006A1C8`,
     `0x8006A1CC`, `0x8006A1D4`, `0x8006A1D8` and the power-up byte `0x800698CC`; the reflection shift (+0x2C) by
     `0x8002A4C8` only on a floor of surface type 4 (`0x8002B078`). Neither was reached: Colossus `start` held Up,
     Glimmer `moby-teleport 554 2`, the story walk; all read 0. The pulsing tints colour (byte 1) from a sine of the
     timer, which `SpyroSurface` steps: expected to step at 30 Hz once reached.
   - Spyro 3's letterbox (`0x8006C64C`, set from the camera controller `0x800144B4` modes 9/10 in state 0xE): not
     reached. Tried the new-game story (states 5/6, word stays 0), Sunny Villa `0/1/0` entry, and `moby-teleport`
     onto every single-instance class in Sunrise Spring `0/0/0` (35, 120, 198, 325, 425, 432, 434, 435, 436, 460, 498,
     514, 516, 527, 561, 651, 683, 942, 983, 1002, 1023; 211, 416, 563 by index), with and without Triangle: none
     opened a conversation (436 and 460 are portals into a level load).
   - Spyro 3's story (state 6, handler `0x8001E2A8`): shown. Three real-time shots (`s3cut.py s3c3 12 6 60 4700`,
     dumps at steps 2850, 3550, 4330): every in-between composed with all tracks paired, each in-between 0.55-0.65 of
     its neighbours' distance from each (`between.py`), Bianca and the egg moving in steps through the run
     (`s3c3-3550-bianca.png`).
   - Spyro 2's story (state 6): its rain steps (issue 0242); the rest of the shot is still.
   - S2 0x12 and S3 0x12 scene states and the S3 bosses: no route reached them.
## Fixed (2026-11-11, shown in frames, `scratch/hudslide3`)
- Spyro 3's sky object wave. Cause: `titles/spyro3` gave `SkyWave` no facts, so the far colour `0x8001EDEC` stores
  in `0x8006EDA0` for `0x8001C8C8` was the later tick's in every in-between. `kSkyWave` installs the shared
  consumer override (plan Open 7). Sunrise Spring `0/0/0`, no input (`pg_portal.txt`, `portal_wave.png`): the far
  Sunny Villa portal is drawn in that colour (IR0 0xFFF); over a whole wave cycle its 5-bit level follows the drawn
  value, changes on in-betweens 5 of 9 times, and four in-betweens keep their earlier real's level while the later
  real's has moved on. Leak: the HUD route below ran with it installed, `fielddigest` identical.
- Spyro 3's HUD slides. Cause: `0x80029708` (the widget's icon) and the digit drawers `0x800291B8` / `0x800293C4`
  read the slide table `0x800674E8` at the widget's slide byte +0x3F inline, and the replay blended nothing they
  read, so each in-between drew the later tick's slide. All three are native (`titles/spyro3/render/hud_slide.*`):
  inside a replay the icon and each digit are placed between where the two ticks' slides put them (`HudSlide`,
  widget records `0x80067248`, 8 of 0x54, flags +0x18, slide +0x3F); the slide the icon step hands its digit
  drawer is carried with the earlier tick's, keyed by the caller's $sp and the replay run. The 1.5-row half step
  is kept: the icon emitter `0x800289C8`'s POLY_FT4 corners are stored that far below their rows. Sunrise Spring
  `0/0/0`, Start tapped (`p3hud.png`, `edges.py`): the gem count's and the lives digit's lowest rows move 12 of
  2160 rows every present (the tick's 3 native rows at 8x, halved), reals and in-betweens alike, through the bounce.
  Real frames (fps60 off, 4:3, `PSXPORT_OVERRIDE_DIFF`, every call): `0x80029708` 1020 of 1020 and `0x800291B8`
  1020 of 1020 match the guest (`diff3.json`); `0x800293C4` was called by no screen reached and is matched to the
  binary by reading. Leak: same route, fps60 on (678 in-betweens) and off, `fielddigest` identical over 2680 fields.

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
