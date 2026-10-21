# 0196: gameplay in-betweens judder

Status: open

## Observed
USER, 2026-10-20: "there is a ton of interpolation judder" (Spyro, fps60 on, real window).

The play checks of 2026-10-16 (0191-0193) passed the vsync and frame-order bars in gameplay, so the
judder is not a missed present or a repeated record. It is what an in-between shows: a position that
does not lie between frame n-1 and frame n for some object, or the camera and an object blended on
different terms.

## Cause
`psxport runtime/psx/present/frame_composer.cpp:composeFrame` kept an object's entries from frame n
when its in-between render sampled a CLUT frame n never sampled, and kept every unowned primitive
as frame n drew it, so one present mixed lerped and unlerped primitives.

## Change so far (B4)
`composeFrame` reads a CLUT frame n never sampled from the device VRAM, and an in-between with any
primitive no producer owns, or any render it cannot place, is not composed: frame n is shown whole.
This applies to every title with StateProducers (Crash Bash, Tomba and Toy Story 2 too).

**Regression:** this turns gameplay interpolation off rather than fixing it. Every gameplay
in-between in Spyro 1, 2 and 3 now repeats frame n, so fps60 shows 30 fps motion; Spyro 1's actors,
sky, glows and other owned objects, which used to move on in-betweens, now never do. The judder is
gone only because nothing is interpolated.

## Evidence
Spyro 1, `warp 10`, fps60, hidden window, 40 consecutive presents, mean absolute difference of
adjacent presents: before, alternating about 1.3 and 12.9 (in-betweens nearly frame n with some
objects moved); after, 0.0 and about 13 (in-betweens are frame n whole). Spyro 1/2/3 at 16:9 and
4:3 after the B4 fixes (12 presents while turning): every in-between 0.0 from its frame n. Real-time
check by the operator not done.

## Spyro 1 (2026-10-21)
Cause of the repeated frames: GamestateDraw `0x8001ED5C`'s callees the portal drawer `0x80050BD0`
and the moby shadows `0x80059F8C` drew unkeyed packets in every gameplay frame with a portal or a
shadowed moby in view (2986 unkeyed over the B4 census run, not per frame), so `composeFrame` showed those frames
whole.

Fix: both are native producers (issue 0186): `spyro1::portals::PortalProducer` (fill and near sky,
per portal) and `spyro1::shadows::MobyShadowProducer` (per moby record).

Proof, fps60, 16:9, hidden window, `PSXPORT_PRODUCERS=1`, 40 presents while turning after `warp` 10,
11 and 20 and beside level 10's portal: keyed without render 0 and no unkeyed primitive after
gameplay starts; adjacent differences even (level 10 about 7..10, level 20 about 6..12, the
portal run 6.5..9.9, against 0.0 and about 13 before); the image shift per present steady (level 11
-5/-6, level 10 -2..-5); each in-between between its neighbours (|p - prev| 9.47 and |p - next| 9.68
against |prev - next| 13.43 on level 10, both parities alike); real frames pixel-identical to the
guest-drawn run; no crack in the near sky's in-betweens; the HUD anchored and turning between
frames. Captures: `scratch/interp/final{10,11,20,near,hud}/` (`spyro1-seq/p*.ppm`).

## Next
Spyro 2 `0x800156FC` and `0x80011E9C` plus span misses, Spyro 3 span misses (issue 0176): their
in-betweens still repeat frame n. Spyro 1's horn and motion trails and demo overlay (issue 0186)
show their frames whole when drawn. A real-time check by the operator is still to do: these runs
measure the composed images, not the play feel.
