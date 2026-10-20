# 0201 — Spyro 2 and 3 panel logos are not located

**Status:** open. **Owner:** `game/core/title_logo.cpp` (`spyro::Runtime::logoFacts` answers "no logo" for both).

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
