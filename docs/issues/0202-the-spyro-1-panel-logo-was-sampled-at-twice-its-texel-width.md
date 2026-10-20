# 0202: the Spyro 1 panel logo was sampled at twice its texel width

Status: fixed

## Observed
The picker's Spyro 1 panel showed the SPYRO THE DRAGON wordmark squeezed to half its width, 128x128,
and the logo fact clipped its 255-texel sprite record to 128 columns to hide what the right half drew.

## Cause
`game/core/title_logo.cpp:extractPanelLogo` fetched an 8bpp texel from halfword column
`pageX + u` (byte `u & 1`) and a 4bpp texel from `(pageX + u) & ~7` (nibble `u & 7`), where the GPU
reads halfword `pageX + (u >> 1)` / `pageX + (u >> 2)`. Every other texel was skipped, so the record's
256 texels read 512, and the right half of the record showed the banner's texels.

## Fix
The extractor samples through psxport's `psx::gpu::sampleTexel` with `decodeTexPage`/`decodeClut`
(`runtime/psx/present/vram_pixel.h`), and its local texel fetch, palette read and texpage/CLUT decode
are gone. The `clipWidth` workaround is deleted with it: the fact names the whole record.

## Check
Headless picker run (`picker shot` after `the title's own logo snapshotted`): the log reads
`255x128 over 1 part(s), tpage 0098 clut 7FE0 (14137 of 32640 texels)`, was `128x128 (7067 of 16384)`,
and the panel shows the whole wordmark at its own aspect.
