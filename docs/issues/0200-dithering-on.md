# 0200: dithering is on

Status: open

## Requested
USER, 2026-10-20: "Remove dithering".

The record raster applies gpu.c's 4x4 ordered dither (`shaders_gpu/record.frag`). The product does not
dither: shaded and blended primitives keep full colour precision in the host picture. The guest GPU
device keeps its own dither bit as guest state; only the host picture changes. Owned by psxport's record
raster. Prove it with a Spyro 1 sky-gradient screenshot taken before and after.
