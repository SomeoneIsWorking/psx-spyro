# 0201 — Spyro 2 and 3 panel logos are not located

**Status:** fixed (2026-11-03). **Owner:** `game/core/title_logo.cpp`, `titles/spyro{2,3}/core/logo_facts.h`.

The picker panel draws no name for Spyro 2 and 3 because their title wordmarks have not been found
in guest state. Spyro 1's recipe (one record of a 64-entry sprite table drawn by the title overlay,
`titles/spyro1/core/frame/logo_facts.h`) does not transfer.

## Measured on SCUS_944.25 (2026-10-03, paused on its title screen at frame 1019)

- Not a movie: no `[fmv] begin`, no `[cd]` streaming after boot, and no `.STR` directory record in
  either disc image.
- No sprite table: no run of six Spyro 1 sprite records in the 2 MiB of RAM, none in the 34 overlay
  modules indexed at disc 0x23000 (bases 0x8006DA40, 0x8006DB50, ... 0x110 apart), and only four
  record-shaped words in the executable (fixed geometry tables at 0x8006016C, 0x800633BC,
  0x800633D4, 0x80064848).
- Texture memory is populated. `provat` reports `<never written>` for DMA-written pages (it does so
  for Spyro 1's resident logo too), so it cannot answer this.
- The plate (screen rect (160,10)-(360,110)) is not a 15-bit RAM image: a 12-texel run matched
  against the whole saved machine gives no hits.
- 0x8019C000 is a 15-bit, 512-texel-wide staging buffer holding a vertical slice of different
  wordmark art (rows 85..125), not the plate shown.
- The overlay bundle (WAD[0x237DC..0x35800], base 0x8006DA40) is a setup table of pointer stores
  with no `jal`; the emitter is not in it.
- The attract flag 0x80066D40 has no static reference in the analysed program.

## Measured on SCUS_944.67 (2026-10-03)

- The title screen alternates with the demo, so a gate must be guest state, not a frame count.
- No sprite table in RAM; not a movie on the same evidence as Spyro 2; texture memory populated.

## Next step

Search populated texture memory for the plate as an indexed (4- or 8-bit) page with its CLUT, and
name that page's origin, depth and CLUT in a title `logo_facts.h`.

## Fixed (2026-11-03)

The wordmarks are not sprite records or RAM images; they are textured primitives the title screen draws
each frame. psxport's `drawnLogo` rebuilds the logo from the last committed frame record: the primitives
that sample the wordmark's texture, rasterized from VRAM. Spyro 2 draws its plate and banner from the 8bpp
page at (512,256) and is read at game state 11 with title state 2 (`0x80067E0C`); Spyro 3 draws one
semi-transparent 8bpp quad from page (512,0) through the palette at (512,0) (terrain shares the page) at
game state 11 with title state byte 1 (`0x8007F12C`). Spyro 1 moved to the same reader (page (512,256)) and
its sprite-record snapshot was deleted. Shown in `scratch/picker2/logos2` (255x96, 191x75, 256x129) and
`scratch/picker2/cold/open8.png`: a fresh selector draws the saved Spyro 2 and 3 logos on their never-focused
panels (`steps=0 insns=0`).

## Review fixes (2026-11-03, picker3)

- Spyro 3's gate opened on the Insomniac credit: title state byte 1 also runs the credit, at step byte 0
  (`0x8007F12E`); the logo is up at step 1. The gate reads both bytes (`kTitleStepLogoUp`); a gate on byte 1 alone
  saved a 191x3 strip of credit art (`scratch/picker3/p3`, `p4/sheet.png`).
- Spyro 1's gate was `m_CurrentTick >= 1170`, which also holds while sub-state 2 slides the logo in
  (`TitlescreenDraw`); it is now `m_Mode` TSM_Init with `m_SubState` 3, the logo at rest, the same state warp's
  `titleScreenIdle` waits for, which now asks the gate.
- The logos are drawn by gpu.c (psxport `drawnLogo` on a scratch device). `scratch/picker3/r2`: read fresh 255x120,
  255x94, 190x74 on their title screens; `r4/sheet.png`: the next process draws all three from its first frame at
  one texel scale on the unfocused panels.
