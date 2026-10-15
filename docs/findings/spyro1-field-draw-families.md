# Spyro 1 field draw families

Guest facts about Spyro 1's field draw routines, recovered while the native renderer drew them and
kept for their producers (issue 0186). The native renderer was deleted on 2026-10-15; every fact
below is about `SCUS_942.28`, not about port code.

## Which family matters most

Over 1,577 in-between presents of the Artisans replay, 90.1% of the world-layer items the native
in-between could not rebuild came from the terrain-object producer `0x8004EBA8`; the 2D layer was
another 9.8%. Shadows, glow, sparkles, particles and tracers together drew about 32 faces per logic
frame.

## Actors and shadows

- Paired actor `0x80023AC4..0x800258EF`: its local face buckets are coalesced into the world table
  (`docs/findings/paired-actor-world-order.md`). It reads its live GTE rotation back at `0x8002401C`
  and publishes it into `g_SpyroFlame+0xB8` at `0x80024110`, gated on `g_SpyroFlame+0x9A`; the
  flame `0x80058D64` reads that matrix, composed from the camera rotation, `g_Spyro+0x0C`, and
  published before `g_Spyro+0x10` is applied.
- Moby shadow staging `0x8001F344`/`0x8001F350` admits an entry only when `m_ShadowDistance` is
  negative and the view depth is nearer than `0x1200`. The list starts at `0x800724F4`; the shared
  cursor is `0x80075F00`; the consumer is `0x80059F8C`.
- Spyro's shadow `0x80059A48`: the rotation rebuild at `0x80059A9C..0x80059B00` is two MVMVA
  (`0x4A486012`) with a register splice, a rotation about Y. Both GTE input words are built as
  `andi` on the low half plus a plain add (`0x80059AC8`/`0x80059ACC` anchor, `0x80059E2C`/
  `0x80059E30` ring), so the high half never borrows. The fan is suppressed over water by
  `0x8004A088..0x8004A0A8`.
- The same fan is 16 0x28-byte packets with draw mode `E1000640` and flat Gouraud colour
  `0x32608080` around one projected anchor, ordering-table bias 3 on the anchor's SZ; it reads
  SXY2/SZ3 and MAC1-3 without testing FLAG. `0x8007AA10+0x24` gates it; the Moby shadow drawer is
  `0x80059F8C`.
- Paired actor `0x80023AC4`'s colour-fade arm at `0x80024B60` runs every entry of the model's colour
  table through one GTE `INTPL` toward the far colour at `g_Spyro + 0x28`, writes the results to
  scratch and points the ordinary parser at that copy.
- Shaded moby queue `0x80022A2C`: bit 7 of `m_RenderRadius` (+0x50), tested at
  `0x80022B34`/`0x80022B38`, selects a screen-space path at `.L80022D1C` with OFX/OFY set to the
  moby's own x/y (`0x80022D2C`/`0x80022D30`), TRZ = z>>1 (`0x80022D3C`, `0x80022DC4`), a view
  matrix starting at diag(0x1000, 0xA00, 0x1000) (`0x80022D64..0x80022D88`) composed with the
  moby's rotation at +0x44; no camera cull and no shadow. Every `g_Hud` moby and HUD glyph sets it.
  Per-vertex lighting is `0x800230EC..0x80023258` (variant 1 face arm `0x80023574..0x800236D4`).
  Only the lit flat arm (variant 3, `0x80023734`/`0x80023738`) exempts near faces from back-face
  culling; variants 0-2 cull at any depth.

## Effects

- `0x80058BA8` calls the glow renderer `0x800580F4` and then the sparkle renderer `0x800584C4`.
  Glow: sixteen records at `0x80078800`, each a fan of additive Gouraud triangles from a projected
  centre to a ring of black points scaled by radius over depth, four-edge outcode reject against a
  512 right edge, OT bin `>> 7` stepping 0x40 back past 0xFF. Sparkle: eight records at
  `0x80077108`, projected through a diagonal matrix scaled by its own depth so it keeps a constant
  screen size, drawn as two crossed lines; the renderer also writes guest state (burns lifetime by
  `g_DeltaTime` `0x800756CC`, spins the angle byte, kills a sparkle it does not draw).
- The breath flame's face set changes every logic frame (15, 25, 32 faces), so its in-betweens
  cannot pair faces between two frames.
- Particles `0x800573C8`: textured arms project all four corners and sort on the sum of their
  depths; the scan steps over every negative type except `-1`.
- Two field-particle producers wrote a visibility byte into the particle record from the screen
  window, so any widening of that window changes guest state.

## World and sky

- `0x8002B9CC` (SetupFrameOT) clears `0x8006FCF4..+0x1C00` each frame and calls RenderWorldChunks
  `0x800258F0` with an LOD; its final stream is `docs/findings/world-semantic-oracle.md`.
- Cyclorama `0x80050BD0` reuses `0x8004EBA8`; portals use `RotVec8ToMatrix` `0x80016D2C` (Y, then
  X, then Z, each composed on the right) and the camera-facing test `0x80051D98..0x80051E6C`; a
  portal behind the camera is skipped by `0x80051D0C..0x80051E70`. The mask is `0x8004FEA0`, the
  mid-distance portal `0x80050240`.
- The dragon cutscene `0x8001CFDC` (stage 8) switches on `g_DragonCutscene.m_State` 0-7; its burst
  star is `0x80058864`. Moby scale byte +0x57 is applied by the `sra 5` idiom at `0x80022CCC` and
  `0x8001F864`.

## HUD

- Layout comes from the image's tables: `g_HudMobyTargetPos` `0x8006E68C`, `g_HudEggTargetRect`
  `0x8006E71C`, `HudReset`. Gem digits and chest x 46..174 are left-edge, the dragon counter x
  230..292 centred, lives digits and Spyro head x 394..464 right-edge, key x 430 right-edge, the
  treasure row x 36+27i left-edge, the life orbs right-edge (placed from `m_Mobys[10]`).
- The 2D layer is the second ordering table at `0x8007581C` (`docs/re-frontier.md`
  `spyro1.record`); its 13 producer bodies (1,451 words) issue no COP2 instruction. On a field frame
  it draws only behind its gates: `g_Fade` `0x80075918`, `g_ScreenBorderEnabled` `0x8007570C`,
  `D_800756C0`, `g_Hud.m_LifeOrbCount` `0x80077FE0`, `m_EggCount` `0x80077FD4`.
- "DEMO MODE": builder `0x80018908`, call site `0x8001F000`, gated on `g_DemoMode` `0x80075714`,
  string `0x80010AC0`, position (199, 200, 4352), spacing (16, 1, 5120), width 18, shade 2.
- The pause menu stores the frozen frame to VRAM (512, 0) through `0x8001A40C`; the `GS_ExitLevel`
  curtain `0x8001C694` samples it.
