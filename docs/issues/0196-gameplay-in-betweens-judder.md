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

## Spyro 2 (2026-10-21)
Cause: everything but the terrain was guest code with no emission scope (issue 0176), so every
gameplay frame was shown whole.

Fix: `spyro::field_redraw` re-runs the dispatcher `0x800155A0` less the terrain as a guest redraw at
the in-between's camera, mobys and Spyro, and the flatten keys the field's packets and saves the
redraw (issue 0176 has the details). psxport gained `GuestRedraw`, `WriteRollback`, the journal's
`Discard` mode and `StateProducer::composition`.

Proof: fps60, 16:9, hidden window, `warp` 10, 11, 22: no unkeyed primitive after gameplay starts;
adjacent differences even (level 10 11.5..14.8, level 22 2.3..5.9) against 0/19 alternating before;
81..95% of each in-between's changing pixels between its neighbours; HUD anchored. Captures
`scratch/interp2/f{10,11,22}`, `h10`. Level 21 (20 fps logic) shows diff, diff, 0: one in-between per
three fields.

## Spyro 2 effects and animation (2026-10-21, review)
Observed: the redraw moved only the camera, each moby's position, angles and flags, and Spyro's
position and angles, so the effects pass (flag 0x10: 0x8004A8D4, 0x8004AC78, 0x8004B194,
0x8004B72C, 0x8004BAA4 per entry of 0x80069F30, 0x80048128) and every animated mesh drew the next
field's positions and poses under the in-between camera: one present, two moments. Level 11's
adjacent differences kept a short/long pattern (4.3/6.0, 5.6/9.2).

Cause: `field_redraw.cpp:writeInBetween` wrote no effect record and no keyframe pair or progress
byte, so the guest drew them from frame n+1's state.

Fix: `spyro::effect_pools` reads each pool once per field and pairs a slot as one effect while it
stays live with the same kind (glows while their life only counts down, particles while their age
only grows, trails by owner and ring ages); positions, spins, sizes and life move, and only the
changed words are written to the redraw. `spyro::keyframe_blend` moves a pose (moby +0x3C/+0x40,
Spyro's body and head) along its own keyframe time, in the from pair or, when the next pair starts
at the from pair's frame B, in that one. Paired nothing else: a slot reused or an animation switched
is drawn as the next field has it. A first version wrote a step past the row (k = 16 on a 16-step
row became the next row's step 0); the step is now bounded before it is encoded.

Proof, fps60, 16:9, hidden window, same route as the review (`warp` 10, 11, 22, right+up, 40
presents): real presents pixel-identical to the review build (0.00 on every odd present), only
in-betweens changed (0.2..1.5); census unkeyed 32 and span-miss 123 at shutdown (boot), no redraw
warning. Level 11, rows 26..214 (the letterbox edges excluded), last 14 pairs: the review build
alternates 1.14..1.24x, this build 1.05..1.14x. The rest is two overlays that still show the next
field at the in-between, both stepped by logic, not by the dispatcher's inputs: the conversation
letterbox 0x8004D604 grows one whole row per logic frame from its counter 0x8006701C (a row an
in-between cannot split), and the conversation box that logic links into the overlay pair through
FUN_8001B390 before the dispatcher runs is n+1's box, so while it opens its edge matches the next
real present (presents 20/21 and 22/23 equal). With the bars counted the first 20 presents still
alternate about 1.2x. Level 10: 11.73..14.56, even, 81..83% between. Level 22: 2.46..3.13,
a 3.1 pair every third logic frame at Spyro's head (not traced; his logged poses there are at the
in-between's keyframe time). Pictures at present 32 of level 11: the
fairy's wings and Spyro's legs and tail lie between presents 31 and 33 where the review build showed
33's pose (`scratch/s2effects/g11-32-fairy.png`, `g11-32-spyro.png`). Captures
`scratch/s2effects/g{10,11,22}/spyro2-seq`.

## Next
Spyro 2: the opening conversation box is linked by logic before the dispatcher, so the redraw
keeps n+1's box; moving it needs its drawer (FUN_8004D2D8 / FUN_8004A374) re-run at the in-between
in place of logic's packets. Spyro 3: no field producer, so its in-betweens still repeat frame n. Spyro 1's horn and motion trails and demo overlay (issue 0186)
show their frames whole when drawn. A real-time check by the operator is still to do: these runs
measure the composed images, not the play feel.
