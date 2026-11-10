# 0237 — Spyro 2 and 3: what the draw body's replay does not move yet

Status: open. Owner: `game/core/draw_section.*` (the replayed states), `game/core/draw_motion.*` (the tracks),
`titles/spyro2|3/core/draw_section.*` (each image's switch); plan `docs/plans/spyro23-replay.md`.

## Observed
Since field_redraw and the S2/S3 producers were deleted (2026-11-10), every Spyro 2/3 in-between is a replay of the
draw body 0x800156FC / 0x8001E638. Gameplay moves halfway (Glimmer `warp 0/1/0`, Sunrise Spring `warp 0/0/0`,
`scratch/replay23/s2c`, `s3c`). These do not:

1. **Loading screen, state 5** (S2 0x800151F0, S3 0x80059038): never replayed, held at 30 Hz. S2's handler polls
   until it runs out of budget at 0x800577A0; S3's waits in VSync at 0x8005927C (`scratch/replay23/s3a.log`, 216
   refusals before `replaysState` named the state). A state the switch has no case for (S3 8, 0x11) is not
   replayed either.
2. **Inputs that still step at 30 Hz**:
   - HUD slides (plan Open 8): the slide is an index into a byte table that every slot and digit reads at its own
     offset, clamped at both ends (`0x80052498`), so no table entry can be written for all of them. Shown stepping in
     Spyro 3's pause (`scratch/spyro23-tracks/pause3_hud.png`: each in-between equals the next real). Next: blend
     `0x80052498`'s result per slot inside a replay.
   - Letterbox height (Open 1): drawn as whole GP0 rows, one row per draw; needs sub-row vertex places.
   - Sky object wave: the draw reads its sine at `tick * 4`, which a written tick cannot halve. The objects
     themselves are level data and do not move.
   - The held object's part timers (`0x8006AA3C..`) step; its position is tracked.
3. **Not shown since the change**: cutscenes (6, 0x12) and the boss and scene states: no warp reaches them yet. Spyro
   2's water floor, reflection and tint (now tracked by `SpyroSurface`) and the held object are tracked but no
   frames show them: Glimmer and Sunrise Spring do not reach water or the held effect by warp.

## Fixed (2026-11-10, shown in frames, `scratch/spyro23-tracks`)
- Fades: the dispatcher's fade words are motion tracks (S2 `0x80066F68`, `0x80067150`; S3 `0x8006C598`,
  `0x8006C7C0`, red flash `0x8006C6D4`). The Glimmer arrival fade-in alternates t=0.5 / t=1 and each in-between's centre
  brightness lies between its neighbours (6.24 / 9.35 / 13.30, 13.30 / 17.41 / 21.95, 21.95 / 27.20 / 33.32).
- Spyro 2's conversation box: `ConversationSlide` writes the slide table entry state 1's handler reads (mode
  `0x80067134`, timer `0x80067130`). `warp 0/1/0`, `moby-teleport 554 2`: the box's right edge moves 128 of 5472 columns
  per present through the slide and in equal steps through its bounce (`conv2_box.png`).
- Spyro 3's level-load refusal: `0x8001DC5C` in steps 0 and 4 of `0x8006FBC8` with the fade at 0xFF calls `0x8001EBAC`
  (DrawSync, whose GPUSTAT poll at `0x8005C224` never ends in the sandbox, VSync, ClearImage). That frame is no longer
  replayed (`spyro3::DrawSection::replaysState`); the screen is black there. The warp now runs with 0 refusals.
- Title screen (S2): composed in-betweens, Spyro's head turn halfway (`title2_spyro.png`).
- Pause and Sparx's second pass (S3): every in-between composed, Sparx drawn, his wings between their neighbours
  (`pause3_sparx.png`).
- Leak proof: same inputs, fps60 on (S2 920 in-betweens, S3 746) and off (0): `fielddigest` byte-identical over 2914
  and 2813 fields.
- Pacing (host load 2.4-2.7): a paced Glimmer and Sunrise Spring run each show 698 of 701 simulated 60.00 Hz vblanks
  advancing 0.5 tick, 3 repeats (the 59.82 Hz field rate).
