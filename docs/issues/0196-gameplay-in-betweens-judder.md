# 0196: gameplay in-betweens judder

Status: open

## Observed
USER, 2026-10-20: "there is a ton of interpolation judder" (Spyro, fps60 on, real window).

The play checks of 2026-10-16 (0191-0193) passed the vsync and frame-order bars in gameplay, so the
judder is not a missed present or a repeated record. It is what an in-between shows: a position that
does not lie between frame n-1 and frame n for some object, or the camera and an object blended on
different terms.

## Next
Reproduce on `warp 10` with fps60 per title. Find the drawn object whose in-between motion is not
monotonic between its two records, and trace it to the producer or the pairing that places it. Per the
interpolation contract: identity from a draw-site tag, transforms lerped, never screen vertices, and a
frame with any unowned entry is shown as frame n whole.
