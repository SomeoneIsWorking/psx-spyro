# 0218 — Spyro 2 and 3 sky leaves the 16:9 margins empty

Status: fixed (2026-10-28). Owner: `game/render/field/guest_sky_drawer.*`.

## Observed
16:9, hidden window, `warp 42` on Spyro 3: the sky in both margins is the flat clear colour (bright
green) with jagged edges that stop at the 4:3 picture's edge; `warp 21` shows flat blue there. Spyro 2's
Insomniac intro (1200 steps from boot) shows black holes in the left margin's sky, and `warp 31` a flat
dark band.

## Cause
`FUN_80029170` (SCUS_944.25) and `FUN_8001C368` (SCUS_944.67), the same instructions, outcode each sky
vertex with retail's x tests (`sxy << 16 <= 0`, `(sxy << 16) - 0x2000000 >= 0`) and drop a sector or
triangle whose corners are all past one edge, so every sky triangle wholly inside a margin is never
drawn.

## Fix
The drawer is native (`spyro::guest_sky::draw`, installed by `registerEngineRenderOverrides`) and tests
the presented window (`presentedBounds`), as the terrain drawer does; its sector-visibility leaf is
still the guest's. At 4:3 it equals retail: `PSXPORT_OVERRIDE_DIFF=spyro2-sky-drawer` 34/34 match on
`warp 12`, `spyro3-sky-drawer` 29/29 on `warp 42` (registers, GTE and RAM).

## Shown
The same 16:9 captures after the fix: sky drawn to both screen edges on Spyro 3 `warp 42` and `warp 21`,
Spyro 2 `warp 31` and both intros.

Real time, paced, 16:9, fps60=1, ires 0, sink 3840x2160 (2026-10-29, load under 2): Spyro 2 `warp 12` 2,262 present
intervals, 2 missed (0.09%); Spyro 3 `warp 42` 2,187 intervals, 2 missed (0.09%); the worst interval in each is the
warp's load hold (`scratch/review-fixes/s2paced`, `s3paced`).
