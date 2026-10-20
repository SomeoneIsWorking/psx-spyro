# 0200: dithering is on

Status: fixed

## Requested
USER, 2026-10-20: "Remove dithering".

## Cause
psxport's record raster applied gpu.c's 4x4 ordered dither in `shaders_gpu/record.frag`, so shaded and
blended primitives in the host picture carried the dither pattern.

## Fix
`record.frag` no longer dithers: shaded and blended primitives keep full colour precision in the host
picture. The guest GPU device keeps its own dither bit and VRAM as guest state, so `recordcheck`
mismatches dithered draws by design (psxport `docs/presentation.md`).

## Proof
Spyro 1 `warp 10`, 16:9, one frame read both ways (`scratch/b1/s1_dither/`): over the sky
(240x38 pixels) the device's VRAM changes colour between horizontal neighbours on 64.3% of pixels
with a visible checker (`device_sky.png`); the presented record picture changes on 14.4% and is a
smooth gradient (`record_sky.png`).
