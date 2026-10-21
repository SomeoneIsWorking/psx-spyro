# 0196: gameplay in-betweens judder

Status: fixed

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

## Fix
`composeFrame` reads a CLUT frame n never sampled from the device VRAM, and an in-between with any
primitive no producer owns, or any render it cannot place, is not composed: frame n is shown whole.

## Proof
Spyro 1, `warp 10`, fps60, hidden window, 40 consecutive presents, mean absolute difference of
adjacent presents: before, alternating about 1.3 and 12.9 (in-betweens nearly frame n with some
objects moved); after, 0.0 and about 13 (in-betweens are frame n whole, an even 30 fps cadence,
never mixed). Spyro 2 and 3 presents are unchanged. Real-time check by the operator pending.

## Remaining
In-betweens repeat frame n in gameplay because every frame holds unowned primitives: Spyro 1
GamestateDraw `0x8001ED5C` callees (2986 per frame, issue 0186); Spyro 2 `0x800156FC` and
`0x80011E9C` plus span misses, Spyro 3 span misses (issue 0176). 60 fps motion needs those producers.
