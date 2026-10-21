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

## Spyro 3 (2026-10-21)
Observed: fps60, 16:9, hidden window, `warp` 10, 40 presents while turning: adjacent differences
0.0 and 8.9..18.8 alternating; census span-miss 554758 of 1267963 primitives (only the terrain
keyed), so every in-between was frame n whole.

Cause: SCUS_944.67 bound no field facts, so `spyro::registerEngineRenderOverrides` installed only
the terrain producer and the plain flatten; everything the dispatcher `0x8001E460` draws besides
the terrain (mobys, Spyro, effects, HUD, sky, fades) carried no key.

Fix: `spyro3::kFieldFacts` (`titles/spyro3/render/render_facts.h`) binds the shared
`spyro::field_redraw` to this image: the dispatcher `0x8001E460`, its letterbox `0x8001FF44`,
the draw's camera copy `0x8006E02C`, Spyro at `0x80070328` and the effects pools. The trail
pool is the one layout that differs (0x6C records, head and count at +0x64, up to 16 points,
against Spyro 2's 0x30, +0x28, 6), so `effect_pools::Facts` carries the record size and head
offset and reads and writes each trail at the image's stride.

Proof (fps60, 16:9, hidden window, 40 presents each): `warp` 10, 11, 20 while turning, and
level 10 from 40 fields after arrival with the HUD in. Census: span-miss stays at the
pre-gameplay count from arrival to shutdown on every level (10: 9826, 11: 10246, 20: 10034),
unkeyed 0, keyed without render 0. Adjacent differences even (10: 5.9..13.7, 11: 6.1..9.8,
20: 5.6..10.1, 10 early: 6.4..13.7); each in-between 0.89..1.25x as far from its previous present
as from its next, each about 0.7 of the distance between them, and 84..93% of its changing pixels
between them. Real presents are pixel-identical to the previous build (0.00 on every odd present
of level 10), only in-betweens changed. Pictures: Spyro's glide pose, Sparx and a gem advance
between presents with no crack (`scratch/s3field/f10-spyro.png`, `g20-sheet.png`); the gem
counter at the left edge, lives centred and eggs at the right edge stay put on in-betweens
while the 3D gem icon turns (`e10-hud.png`). Spyro 2 level 11 after the trail change:
pixel-identical to the previous build on all 40 presents. Captures
`scratch/s3field/{f10,g11,g20,e10}/spyro3-{seq,early}`, `s2g11/spyro2-seq`.

Left on Spyro 3: the Atlas (Select) is not a gameplay field; its frames show whole (span-miss
grows while it is open). The trail pool was not seen drawing on these routes, so its pairing is
read from the drawer.

## Spyro 1 poses and the intro (2026-10-21)
Observed (USER): the judder is strongest in the intro where the green figure climbs the mountain and
plants the flag (the boot logo intro, gamestate 13 before the title screen). Real speed, fps60,
16:9, shipping settings, unmapped window (`PSXPORT_SHOW_WINDOW=0` read from `/proc/<pid>/environ`),
`PSXPORT_DEBUG=presentseq,compose`: presents strictly alternate in-between and real, no repeated or
backward record, fields 2/2, every in-between composed, intervals even (new-game cutscene: 2939
presents, p50 16.68 ms, p99 16.79 ms). The pacing is not the judder. A stepped capture of the
new-game cutscene shows adjacent differences 0.0 and 4.0 alternating: each in-between equals its
own real frame.

Cause: `titles/spyro1/render/actor_between.cpp:recordBetween` (mobys, their emitters, sprites and
shadows) and `paired_actor_producer.cpp:spyroBetween` (Spyro and his shadow) moved position and
angles but not the pose, so every animated mesh drew frame n's keyframe pair and progress at the
in-between. In the intros the camera is still and the motion is pose animation, so they judder at
30 fps with an exact 60 fps cadence.

Fix: both blend the pose through the shared `spyro::keyframe_blend`, which now takes the progress
encoding: Spyro 1 counts a moby's progress (+0x40, pair at +0x3C) in 64ths (weight k << 6,
0x8001F158) and each of Spyro's three parts (+0x18/+0x1E/+0x24) in 16ths (weight k << 8,
0x80023AC4); Spyro 2/3 keep the nibble form.

Proof (same route, stepped preseq): new-game cutscene adjacent differences even (2.5..2.7 against
0.0/4.0), each in-between as far from its previous present as from its next; real presents
pixel-identical to before (max 0.0), only in-betweens changed. Boot logo intro: 2.2/7.3 alternating
became an even 3..5. Gameplay `warp 10` while turning, Spyro's region: |in-between - previous|
12.01 and |in-between - next| 8.47 before, 10.71 and 10.43 after. Real speed, final build, boot to
the title screen: `[playcheck] presents: intervals=2445 p50=16.75ms p99=17.00ms worst=47.71ms
missed=4 (0.16%)`, `records: repeats=0 backwards=0`. New-game cutscene after: p50 16.69 ms, p99
17.27 ms, irregular intervals only at the cutscene's end and the level load. fps60 off for
comparison: intro p50 33.37 ms, gameplay 608 presents in 20.25 s, all 33..34 ms.

Also found on these runs:
- "THE ADVENTURE BEGINS" card runs logic every field and still got an in-between, so it presented
  at 120 Hz (8.3 ms). psxport `FramePresenter::presentRecords` now gives a one-field logic frame
  one present (psxport issue 0168); the card presents at 16.7 ms.
- The title screen after the flag shows every in-between whole (211 in-betweens in the final boot
  run; packets 0x80187BB4/0x801A3BB4 from the title overlay's logo sprite drawer): issue 0206.
- Gameplay `warp 10` while turning at real speed: logic held 28.2 Hz (missed 3.91%, intervals 9..35
  ms). The main thread was the limit: composition 43%, of it HostMemory's `std::map` lookups 23%
  self time. With `HostMemory` on a sorted vector and a last-range cache, 29.95 Hz, missed 1.08%,
  but intervals still spread 8..30 ms: the in-between half (guest logic, draw, compose, build) runs
  past its 16.7 ms deadline (462 of 620 in-between presents had no wait left) while the real half
  has 6.9 ms spare. Owner and proposed fix: psxport issue 0168.

Not measurable from a tool run: the unmapped surface is never scanned out, so where each present
lands against the display's vblank, and any repeat/skip from pacing to 59.94 Hz on a 60.00 Hz
display under MAILBOX, cannot be seen. The operator's real-window check is still the only test of
that.

## Next
Spyro 2: the opening conversation box is linked by logic before the dispatcher, so the redraw
keeps n+1's box; moving it needs its drawer (FUN_8004D2D8 / FUN_8004A374) re-run at the in-between
in place of logic's packets. Spyro 1's horn and motion trails and demo overlay (issue 0186)
show their frames whole when drawn; so does its title screen (issue 0206).
A real-time check by the operator is still to do: these runs
measure the composed images, not the play feel.
