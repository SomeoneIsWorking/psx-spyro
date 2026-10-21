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
pixel-identical to before (max 0.0), only in-betweens changed. Boot logo intro at product frame 600:
2.2/7.3 alternating became 1.4..3.3, still alternating in places (1.8 3.3 1.4 2.9 at the window's start);
see "Flag intro margins" below. Gameplay `warp 10` while turning, Spyro's region: |in-between - previous|
12.01 and |in-between - next| 8.47 before, 10.71 and 10.43 after. Real speed, final build, boot to
the title screen: `[playcheck] presents: intervals=2445 p50=16.75ms p99=17.00ms worst=47.71ms
missed=4 (0.16%)`, `records: repeats=0 backwards=0`. New-game cutscene after: p50 16.69 ms, p99
17.27 ms on that run; this claim did not hold under host load (psxport issue 0168). fps60 off for
comparison: intro p50 33.37 ms, gameplay 608 presents in 20.25 s, all 33..34 ms.

Also found on these runs:
- "THE ADVENTURE BEGINS" card runs logic every field and still got an in-between, so it presented
  at 120 Hz (8.3 ms). psxport `FramePresenter::presentRecords` now gives a one-field logic frame
  one present (psxport issue 0168); the card presents at 16.7 ms.
- The title screen after the flag shows every in-between whole (211 in-betweens in the final boot
  run; packets 0x80187BB4/0x801A3BB4 from the title overlay's logo sprite drawer): issue 0206.
- Gameplay `warp 10` while turning at real speed: the first figures recorded here (28.2 Hz, intervals
  8..35 ms, 462 of 620 in-betweens with no wait left) came from runs that overlapped heavy host load and
  are withdrawn. The review build at low load gave 1206 of 1216 intervals in the 17 ms bin. The real cause
  of late in-betweens under load was the presenter giving guest logic only the in-between's half of the
  frame: psxport issue 0168, fixed there (presents scheduled at their deadline while the guest runs).

Not measurable from a tool run: the unmapped surface is never scanned out, so where each present
lands against the display's vblank, and any repeat/skip from pacing to 59.94 Hz on a 60.00 Hz
display under MAILBOX, cannot be seen. The operator's real-window check is still the only test of
that.

## Real speed after psxport 0168 and issue 0206 (2026-10-21)
Spyro 1, fps60, 16:9, real speed, unmapped window (`PSXPORT_SHOW_WINDOW=0`, no `PSXPORT_NOPACE`, both
read from `/proc/<pid>/environ`), host load average 2..15 from other agents, `scratch/judder-fix/`:
- New-game cutscene, records 1700-3100: 2802 presents, p50 16.68 ms, p99 16.77 ms, worst 16.96 ms; every
  present within 0.6 ms of its deadline; 1401 of 1401 in-betweens composed. Run end `[playcheck] presents:
  intervals=5433 p50=16.75ms p99=17.00ms worst=45.27ms missed=6 (0.11%)`, repeats 0, backwards 0.
- Same cutscene with 8 busy processes added: p99 16.84 ms, one 57 ms stall; with 14: still late (the host
  is oversubscribed), 616 intervals over 20 ms against 733 before.
- `warp 10` turning, 20 s: 1218 presents, p50 16.68 ms, p99 16.86 ms, worst 18.28 ms.
- Boot to the title, 40 s: 1008 of 1008 in-betweens composed (583 of 1004 before issue 0206's fix), p99
  16.78 ms. Stepped capture of the logo sliding in: its lowest edge moves 4,5,5,5,5,6,5,6,5,6,6,7,6 rows
  per present, against 0 and about 11 alternating when the title's in-betweens were shown whole.

## Flag intro margins (fixed above 1x; open at the shipping 1x)
Reproduction: stepped from boot, `scratch/judder-boot/boot_seq.py <tag> 600` with `SEQ_COUNT=40`, 40 presents
from product frame 600, fps60, 16:9; `parity.py` and `halfway.py` over the captures. At 1x and at
`PSXPORT_IRES=4` the right-margin blocks (x 576..640, y 80..128), x 0..48 y 96..144 and the pole's foot changed
only between an in-between and its real present (0 against 28..42 per block): the sky and the white peak in it
moved only on real presents. The sky producer's render did interpolate (its in-between view rotation lay
between the two fields'), yet every one of its 716 polygons came out on the pixels frame n drew.

Cause, two roundings of one in-between camera:
- psxport `frame_composer.cpp:composeFrame` placed rendered polygons at the GTE's whole-pixel RTPS output; the
  sky turns about one native pixel per logic frame, so no whole pixel lies halfway (psxport issue 0169).
- `game/render/terrain/guest_camera_builder.cpp:interpolate` rounded the in-between camera angles to whole guest
  units; the intro camera turns three units per field, so the in-between sat 2/3 of the way, not 1/2.

Fix: each producer declares the unrounded place of every vertex it copies from a projection into a packet,
by packet address and vertex index, and the composer gives those polygons their fraction (psxport issue 0169).
Spyro 1 declares places in the sky (`Cyclorama`), the moby emitters (`ActorEmitter`, `LitActorEmitter`) and all
terrain passes (`TerrainFrame::declarePlaces`, kept per scratchpad word as each pass projects, carried through
the split grid's pulls and the re-split midpoints). A first version matched places by the packed SXY value;
review rejected that as guessed identity and it is gone. `guest_camera::interpolate` returns angles with four bits below the guest's unit and the builder
reads its tables at that precision (`AngleTable::atFine`); a whole angle reads exactly what the guest reads,
so real presents are unchanged.

Proof (`scratch/judder-boot/{before4,fix4,after1,tr4}`): with declared places (`tr4`) the worst block
parity is 1.9:1, the same blocks and ratio as the value-matched version (`fix4`); with only the sky declaring
(`id4`) it was 4.4:1 along the sign moby and 3.0:1 on the right-hand peak. At 4x, in-betweens within 15% of one neighbour on a moving
16-pixel block fell from 30.3% to 4.2%; the margin blocks went from 0/38 to at most 1.9:1 between the two
present parities; |in-between - previous| / |in-between - next| per in-between 0.62..2.13 before, 0.84..1.60
after. At 1x every real present is pixel-identical to the previous build.

Open at 1x: the shipping settings run at `ires=1` (`cv_ires` defaults to 1, "Vanilla"; Auto is 0), not at auto
resolution as this note first said. At 1x a sky moving about one native pixel per field has no pixel halfway,
so the margins keep up to 4.5:1 parity imbalance there; no composer change can place half a pixel on a 1x
target. Closing it means presenting above 1x by default (Auto or 2x), which costs GPU time on every title and
is the operator's call.

## Moby animation switch (fixed)
Reproduction: from boot to the save picker (`scratch/judder-boot/picker_seq.py <tag> PSXPORT_IRES=4`), 120
presents captured. The picker repeats its loop every 86 presents; once per loop an in-between was pixel-identical
to the real present after it (`live6/picker` p0004, `live7/picker2` p0058), and a hash of the composed record
matched the real one at fences 1545 and 1631. Spyro's moby switches animation there
(`24230505/32 -> 36350404/0`: frame 5 steps 0x23..0x24 to frame 4 step 0x35), so the whole moby jumped on the
in-between and held on the real present.

Cause: `titles/spyro1/render/actor_between.cpp:recordBetween` took `to`'s pose whenever
`keyframe_blend::between` found the two poses were not one animation a step apart.

Fix: across a switch the in-between blends `from`'s nearer keyframe into `to`'s by the guest's own frame
pair (`keyframe_blend::across`; the guest blends two frames itself at its own transitions), when the two frames
share vertex counts and shifts, which the emitter needs since it reads both streams with frame A's.

Proof (`scratch/judder-boot/live10`): no in-between equals a neighbour in 104 picker presents; at the switch
the in-between differs from both neighbours on Spyro (17% of changed pixels equal the previous, 28% the next).
Spyro himself (the paired actor) never hit this: across 1200 part blends while starting and stopping a run in
`warp 10`, every pair continued, since the guest cross-blends his animations itself.

## Terrain and sky identity in `warp 10` (fixed)
Reproduction: `scratch/judder-boot/gameplay_seq.py <tag> PSXPORT_IRES=4` warps to level 10 from the title, waits
120 fields, captures 160 presents and holds right+up for 1.2 s. One in-between about 0.9 s into the turn matched
the next real present on 94% of the changed pixels (`live11` p0052, `live12` p0054, `live13` p0056): terrain and
sky at the next camera, the mobys between. The composer found no earlier state for terrain `0x800258F0` and sky
`0x8004EBA8` object `0x11` at that record.

Cause: `titles/spyro1/render/terrain_image.cpp:registerOverrides` and `sky_producer.cpp:registerSkyOverrides`
named the object by `$a0`, which `FUN_8002B9CC` sets to the word at `0x80076E24` while that is under
`0x800785B4` and to -1 otherwise; the drawer's argument changed, so the field's one terrain became a new object
with nothing to interpolate from.

Fix: both register with psxport's `Arg::None` (one object per call); the argument stays in their saved state,
and the in-between draws with the later field's, as before.

Proof (`scratch/judder-boot/live14`): over 110 presents of the same route no in-between matches a neighbour on
more than 59% of changed pixels, and those (p0066..p0074) sit at half the neighbours' shift (-1 of -2 4x pixels).

## Boot walk, 2026-10-21
Each screen from boot through 20 s of `warp 10`, fps60, 16:9, shipping settings; stepped and live captures
at `PSXPORT_IRES=4` under `scratch/judder-boot/`. Real-speed runs read `PSXPORT_SHOW_WINDOW=0` and no
`PSXPORT_NOPACE` from `/proc/<pid>/environ`; each figure carries the host load average.
- First logo field (fence 8): fixed. It was 3.4..10.5 ms late in every run: `Disc::open` ran on the first CD
  read inside the field (`chd_open`, 1.3..6.7 ms). The disc now opens when the title boots (psxport issue
  0170). `final1` (load 5..10): 0.19 ms. Under load 13..17 (`final3`, `final4`) 2.9..3.9 ms, from the first two
  cold hunks decoded on a busy host (4.8 and 13.2 ms for hunks that take 0.45 ms idle).
- Logo load stages (fences 9, 226..228): fixed. `psx::cd::Disc::residentHunk` decoded every CHD hunk on the
  guest thread into a one-hunk cache, 17..19 hunks per stage, 9..16 ms inside a field. `HunkReader` decodes
  ahead on worker threads (psxport issue 0170). `final1`: 0.25..0.33 ms late; the caller decoded or waited on
  180 of 13789 hunks over a 140 s route, 58 ms in all.
- Boot hands over to the frame loop (fences 436..438): partly fixed. `game/core/archive_transfer.cpp` wrote each
  WAD byte through `Core::mem_w8`, one executable-write invalidation per byte; it now writes the block with
  `Core::mem_write` (one invalidation per load). `final1` (load 5..10): 1.4, 2.4, 0.6 ms late. Under load
  13..17: 437 at 0.9 and 16.2 ms. The rest of that field was `Core::mem_write`'s per-byte store bookkeeping,
  now one span (pass 3 below); what remains is the first Lightrec translation of the loaded code and its SHA-256.
- Raw-to-record switches (437/438, 1834/1835, 2218/2219, 3690/3691): not repeats. The `presentseq` label printed
  `shown=` the presenter's current record for a raw present, while the picture is the displayed VRAM; it now
  prints -1 there (psxport `FramePresenter::presentCurrent`). Stepped at the handover, luma rises 0, 1.14, 2.83,
  4.08, 5.57 over 436..439; at real speed the raw and composed presents at 1830/1831 and 2214/2215 differ (0.004
  inside a slow fade, 0.788). What remains real: at a switch from a raw picture to a record no in-between can be
  composed (there is no earlier record), so the raw picture holds two fields once. `final1`: repeats 0,
  backwards 0 over 4819 records.
- Flag intro: fixed above 1x, open at 1x (above).
- Title screen: the logo and PRESS START are title sprites (issue 0206); PRESS START is a brightness toggle.
  Title Spyro's in-between pose is exactly half way (64ths); his head moves under one 4x pixel per present.
- Save picker: fixed above (animation switch); `across` now takes its step from `keyframe_blend`'s own
  `nearestStep`. The picker and new-game screens run one field per logic frame and need no in-between.
- New-game intro: the dragon and the camera move about one 4x pixel per present; no whole-object holds.
  `final1` (load 5..10): 28 of 2852 presents over 4 ms late, worst 15.7 ms. The main thread ran for about half
  of the 45 ms before the worst ones (46..54 samples at 2 kHz), so it was off the CPU, not composing; with the
  host loaded this is no pacing evidence.
- `warp 10`: fixed above (terrain identity); 4x turn in Artisans (`wk4/walk`) shows no seams in the split ground.
  Spyro himself, shadows, particles, glows, the sprite queue and portals stepped on whole native pixels above 1x;
  they declare places since pass 3 (below).
- Run end, `final1` (load 5..10): `[playcheck] presents: intervals=8424 p50=16.75ms p99=20.25ms worst=73.82ms
  missed=9 (0.11%)`; boot through the new-game card: no present over 4.7 ms late.

## Pass 3, the player's settings (2026-10-21)
`psxport_settings.ini` as the player has it (ires 0, aspect 3, fps60 1), copied to
`scratch/judder-pass3/player_settings.ini`; hidden window, no `PSXPORT_NOPACE`, both read from
`/proc/<pid>/environ`. ires 0 against the 2400x1800 unmapped sink resolves to 8x (captures 4096x1920) and aspect
auto to the sink's 4:3. Stepped captures under `scratch/judder-pass3/<tag>/seq`, from `warp 10` holding right+up
(`gp10-*`), circle (`fl10-*`) or a circle tap (`ft10-*`); each producer's run is compared with the run before its
change, which is otherwise pixel-identical.
- Boot handover (fences 436..438): fixed. Cause: `psxport runtime/psx/core/guest_memory.cpp:Core::mem_write` booked
  every byte of a block store (census, emission note, rollback) through `mem_w8`. It now books the span once
  (psxport `b32b8904`, issue 0171). `final2` (load 1.9..2.7): 436/437/438 at 3.02, 0.74, 0.21 ms late; `prof1`
  (load 2.7..8.3): 0.44, 1.32, 0.29 ms.
- Logos, flag intro, title, picker, new-game intro: nothing new found at 8x (flag intro worst block parity 2.2:1;
  title PRESS START is its brightness toggle).
- Spyro (paired actor): fixed (`8d5124d`). His corners took whole-pixel SXY words: at 8x his right edge sat on
  x = 7 mod 8 in every present (2255, 2247, 2239, 2231) and his bottom stepped 1191 -> 1199 -> 1207 with two-present
  holds. With places the residues are arbitrary and the bottom moves 1..2 px a present (1194..1214). The water cut
  carries its crossings on the places (`cutEdge`) but no route here reaches water.
- Sprite queue (HUD digits, gems' sprites): fixed (`c334fbd`). The "2" digit's left edge stepped 176, 184, 192, 200,
  208 with holds; now 183, 182, 182, 181, 180 .. 208.
- Particles: fixed (`b20aa0b`). Smoke from a flame tap (`ft10`) differs only on the puffs' edges; quads take their
  RTPT/RTPS places; lines, dots and scaled quads in the review below.
- Drop shadow: fixed (`36ca133`). Its lowest dark row was 3 or 7 mod 8 in 9 of 14 presents with holds (1267,
  1263, 1263, 1259, 1263, ..); now 1267, 1267, 1266, 1269, 1267, 1267, 1258, 1252, 1254, 1255, 1247 ...
- Moby shadows: fixed (`b775700`); differences only on the far discs (100..1300 px a present).
- Glow fans (Sparx's glow): fixed (`b44e22d`, rims corrected in the review below).
- Portals: fixed for the fill (`63ba7a2`). The fill's lowest row held on 7 mod 8 (903 x4, 895 x3, 887 x4, 879);
  now 920, 910, 914, 910, 909, 907, 904, 901, 899, 895, 893, 891, 889, 887, 885, 883. The clip's crossings are
  computed on the outline's places. The near sky through a portal carries places the same way but was not
  reached (no route within 0x4000 of a gate).
- Flame `0x80058D64` (`r_flame.s`, `g_SpyroFlame`; open-spyro's DrawSpyroHornStrikeTrails), drawn by
  ComposeFrameScene `0x80019698` while `0x80078760` is set: open. It has no producer, so its packets are unkeyed and
  every in-between while it burns is shown whole: `fl10` (circle held), fences 1553..1575, 23 of 23 in-betweens
  `composed=0`, compose reason `entry N unkeyed`; the census gives 1087 unkeyed primitives, all under GamestateDraw
  `0x8001ED5C`. The native renderer ported it once (issue 0103: `40cd7a4` spyro_flame_recipe, `9c0bcfa`
  spyro_flame_submitter, `c6545a5` spyro_flame_matrix; deleted with that renderer in `cda8544`). That recipe is the
  starting point for a native `0x80058D64` (eight parts: a tip fan of four G3 and a GT4 ribbon down the part's arc
  path at `0x8006DAA8`, linked by depth) with a producer that saves the anchor `0x800786C8`, the published matrix
  `+0xB8..+0xC8` (still written by `PairedActor::orientParts` at `0x800240F8`), the part bytes `+0x20`/`+0x28` and
  the arc entries, lerps the entries when a part's length and start match, and declares the places of the tip
  RTPS/RTPT and each ribbon RTPT pair.
- Level arrival: the first in-between after `warp 10` arrives 7.4..9.6 ms late (`prof1` fence 4083, `final2` 4073,
  load under 3); of its 48 main-thread samples about 15 are Lightrec translating the level's code for the first
  time. Open.
- Run end, `final2` (load 1.9..2.7, the first unloaded host of this pass): `[playcheck] presents: intervals=8426
  p50=16.75ms p99=17.00ms worst=50.47ms missed=2 (0.02%)`, records repeats 0, backwards 0; four presents over 2 ms
  late (226, 436, 4073, 4816). The 50 ms interval is the load field before arrival (`fields=0/1`). `final1` an
  hour earlier at load 3.3..9.8 missed 239; `prof1` at 2.7..8.3 missed 1: the gameplay misses follow the load.
- Every title, 40 s from boot, before and after the psxport changes (`scratch/judder-pass3/titles/`): all exit 0;
  no title gained a repeat, a backwards record or an error. Tekken 3 showed 13 misses after (load 6.3..5.1)
  against 1 before (load 16.7..15.5); rerun at load 6.4..5.2 it missed 0. C-12 missed 95 before (load 15.5..17.4),
  97 after (5.1..5.4) and 19 on the rerun (5.2..4.2): its misses are its own loading, unchanged here.

## Review of pass 3 (2026-10-21)
Same settings and captures as pass 3; `r2-*` under `scratch/judder-pass3/`, each against the HEAD binary
(`r2/spyro_port.head`) on the same route.
- Glow rims: fixed. Cause: `titles/spyro1/render/glows.cpp:GlowFans::draw` gave a rim point the centre's place plus
  its whole-pixel GPF step, so the rim kept the step of IR0 = quotient(size << 12, depth). A rim point now takes the
  centre's place plus its offset times size / depth, before the quotient and the shift truncate it. `r2-glowlog`
  (Sparx): unrounded radius 23.46, 22.97, 24.74 against the guest's 23, 22, 24; the place lies up to 1.18 px past
  the word, which the old clamp cut.
- Sparkle stars: their two G3s are RTPT/RTPS corners and now declare those places (`Sparkles`, `SparkleProducer`);
  the fade byte lerps while it counts down. Places continuous on frames 1435..1530 of `r2-sp10-new`; a star was not
  isolated in a capture.
- Particle lines and dots: lines declare their two RTPS places, dots the place plus their whole-pixel step
  (`particles.cpp`); the rasterizer offsets each line pixel by the place interpolated along it (psxport
  `RecordPlanner::Builder::addLine`). `r2-gp10` (Sparx trail dots), left edge mod 8 over 30 presents: HEAD 600 of
  ~650 at 0; now 88, 75, 93, 89, 100, 63, 102, 81.
- Scaled particle quads: corners carry the unrounded half size (MAC0 / 2^24 times the size bytes) around the centre
  place. No route here draws kind 3 (`r2-kindlog`: kinds 0, 1, 2 only), so the path is code only.
- 1x: `r2-x1-head` and `r2-x1-new`, 20 presents, pixel-identical.
- Near sky through a portal: reached with `gate-teleport 0 0` in `warp 10` (`r2tools/portal_seq.py`, HOLD=left,
  LEAD=6, 24 presents); every in-between composed. Open: at 8x thin seams cross the near sky (orange lines and dark
  ticks off the portal's left edge) on real and in-between presents alike, in HEAD (`r2-gate-head`) and now
  (`r2-gate-new`); none at 1x (`r2-gate-x1`). With the crossings put on the guest's words (`r2-gate-xw`, a scratch
  experiment) the seams vanish but the sky's edge leaves the portal outline. `Portals::crossingPlace` lands a
  crossing on the outline's place line, while `Portals::crossing` (GPL by a 7-bit fraction against the integer
  edge) misses that line by up to 36 px (`r2-crosslog`), and seven guest words in one present hold two crossings
  whose places differ by up to 0.6 px (`r2-skylog`). The owner is the cut (`portals.cpp:Portals::cutShaded`); which
  piece edge opens is not yet traced.
- Water cut: still unproven. Spyro's water word (`0x80078A58 + 0x30`) stayed 0 over 600 frames held in `warp 10`;
  no debug route reaches water yet.
- Timing: every run of this review was at load 13..16 (`uptime`), so no pacing figure here is evidence.

## Pass 4, a 60.00 Hz display (2026-10-21)
Judgement runs `scratch/judder-pass4/realtime.py`: boot, logos, flag intro, title, menus, save picker, new game,
intro cutscene, then `warp 10` turning for 20 s. ires=0 (8x on the 3840x2160 sink), fps60=1, the player's aspect
(settings copied to scratch), `PSXPORT_SHOW_WINDOW=0`, no `PSXPORT_NOPACE` (checked in `/proc/<pid>/environ`).
`simdisplay.py` shows each vblank of a 60.00 Hz display the newest present before it. A present shown more
vblanks than its planned hold (the next present's due step) is a repeat; fewer is a drop. Pass 4 first took the
hold from the present's own `fields=` label, which is the step that led to it, so every planned cadence change
counted as a repeat on the present before it plus a drop on the present itself; the counts below are recounted.

- Pacing. Before (`base0`, load 16 falling to 6): repeat clusters every 16.7 s (8.8, 25.6, 42.3, 58.9, 75.5,
  92.3 s), mean due step 16.68335 ms, playcheck missed=22. Cause: psxport `runtime/psx/frame/pace_plan.h:pacePlan`
  stepped deadlines at the guest field rate (59.94 Hz), so a 60.00 Hz display got one vblank with no new present
  every 1/(60-59.94) s. Fix: psxport 189e5fdc. Deadlines step at the display refresh the field rate locks to
  (`pacedFieldRateMilliHz`). After: mean due step 16.66667 ms, and the clusters are gone. The audio side was not
  shown here (every run had `PSXPORT_NOAUDIO=1`, and a hidden window opens no audio device); the proportional
  steer would have held the queue about 18 ms over its target, which pass 5 removes.
- Census. psxport 0172, fixed by 99b83bb0: the store census arms only under `PSXPORT_PRODUCERS=1`. An armed run
  and an unarmed run are pixel-identical over 4 presents, and the composed in-between counts match base0
  (3618/4830 against 3606/4818).
- Near sky through a portal (`gate-teleport 0 0` in `warp 10`). The owner was not `Portals::cutShaded`. Cause:
  psxport `runtime/psx/gpu/record_raster_setup.cpp:RecordPlanner::Builder::addTriangle` dropped a triangle
  whose native 1x walk was empty. These are clip slivers whose two crossings round to one guest word; their places
  still span area at 8x. Fix: psxport 7936802d draws such a sliver from its places. At 1x the walk is the native
  walk, so nothing changes there. `c-x4`: the seams are gone. What remains is the portal window clamped at the
  512-wide retail clip (x 512..549 in place space), a widescreen gap and not this cut.
- Flame 0x80058D64. Fixed by c88adb6: `render/flame.cpp` `FlameTrails` draws it with declared places, and
  `render/flame_producer.cpp` saves the block and lerps the position, rotation and growing lengths. Override
  differential `PSXPORT_OVERRIDE_DIFF=drawFlame` in `warp 10` with circle held: 24/24 calls match in memory and
  registers. `d-fl`, 24 presents at 8x: every in-between composed (pass 3: 23/23 uncomposed). Flame pixel counts
  rise through the burst with each in-between between its neighbours (463, 422, 461, 521, 560, 966, 1491, 2277,
  3571, ...).
- Sprites. psxport 0169, fixed by ad9d0dd2: above 1x a sprite moves by its declared place. No Spyro 1 producer
  emits sprite codes yet, so `e-x` and `c-x4` differ by 0 px.
- Arrival hold. psxport 0174. The held picture is the one before the load field: the load field itself is
  presented on time, but its due comes 51-72 ms after the previous present because Lightrec compiles the level's
  code synchronously inside it (`lightrec_executor.cpp:LightrecExecutor::executeWithBoundary`). The first
  in-between after it is 11-16 ms late and costs at most one more vblank. Pass 5 shows the held picture is the still
  "THE ADVENTURE BEGINS..." card, so no motion is lost; 0174 is closed on that.

Final build (`final`, the run below; load 2.2-2.5 from boot to cutscene end, 2.4 at turn start, 5.0 at turn end).
playcheck over 8455 intervals: p50 16.75 ms, p99 17.00 ms, missed=6.

| screen | presents | repeats | drops | what it is |
|---|---|---|---|---|
| boot | 56 | 0 | 0 | |
| logos, flag intro, title | 2727 | 1 | 1 | 1210/1211, host noise (absent at load 2 in the review's runs) |
| menus | 320 | 0 | 1 | 1740: a 3-field frame as two 25 ms presents (fixed in pass 5) |
| save picker | 542 | 0 | 1 | 1827: the same |
| new game | 12 | 0 | 0 | |
| intro cutscene | 2924 | 1 | 0 | 3488, host noise (likewise) |
| title after cutscene, warp load | 646 | 3 | 0 | 4086 held 4 vblanks before the load field (0174) |
| `warp 10` turning | 1220 | 9 | 8 | all at 133.5-134.3 s, as load rose to 5.0 (invalid) |
| turn end | 9 | 0 | 0 | |

437/438 is no judder: 438 is `fields=2/1`, a planned two-field hold of the raw picture 437 at the raw-to-record
switch, and 437 is black (pass 5).

All 11 products built with Clang (0 warnings) and ran 40 s at real speed, all exit 0, at load 6-20 (no timing
evidence). Error lines are unchanged from pass 3: Mega Man X4 has 154 x4-vsync, Tekken 3 has 1 card error, and
Tomba 2 writes no playcheck line in either pass.

## Pass 5, review of pass 4 (2026-10-21)
Same route and rules (`scratch/judder-pass5/realtime.py`, `simdisplay.py` with the planned hold fixed).
- Boot handover 437/438: not a defect. 1x presents of fences 435..440 (`scratch/judder-pass5/handover`): mean luma
  0.50 at 435, 0.000 (max 0) at 436 and 437, then 1.52, 2.36, 3.34 from 438 as the fade-in starts. The two-field
  hold is of a black picture.
- 3-field logic frames (menus 1737, save picker 1824 in pass 4's numbering). Cause: psxport
  `runtime/psx/frame/frame_presenter.cpp:FramePresenter::presentRecords` presented every frame of two or more
  fields as two presents at `t = 0.5` and `t = 1`, so a 3-field frame was two 25 ms presents that a 60.00 Hz
  display shows for 1 and 2 vblanks. Fix: a frame of n fields is n presents one field apart at `t = k / n`
  (`psx::present::inBetweenAt`). `final5`: both 3-field frames (1740, 1826) are three composed presents due 16.67 ms apart
  (`t=0.33333334`, `0.6666667`, `1`). Spyro 2's field redraw saved one in-between at 0.5 only; it now redraws once
  per in-between of a frame as long as the last (`FieldOwner::lastPacedFields`) and refuses a frame of another
  length. Spyro 2 `warp 10` turning (`s2`, load 27-29, content only): 328 of 334 in-betweens composed, all
  two-field frames; no three-field Spyro 2 frame was reached, so its two-redraw path is unexercised.
- Audio speed. psxport `AudioQueuePolicy::consumptionRatio` now takes `FramePacer::pacedSpeed` (paced over guest
  field rate, 1.001 for NTSC on 60.00 Hz) as its base, so production and consumption match at the queue target and
  the full 0.5% trim stays for jitter. Not observable in an agent run: a hidden window opens no audio device.
- Arrival hold (0174). 1x presents from the warp on (`probe-arrival`): the picture held before the load field is
  the "THE ADVENTURE BEGINS..." card (text on black, mean luma 1.8-1.95, max 244, still but for a slow fade); the
  load field shows the first fade-in picture (mean 0.29), and the late first in-between falls in that fade. Nothing
  on screen moves across the hold; 0174 closed with the corrected cause.
- Flame 0x80058D64 differential widened (`scratch/judder-pass5/d-diff`, fps60 off, `PSXPORT_OVERRIDE_DIFF=spyro1-flame`):
  nine bursts in `warp 10` (plain, turning, four with the fairy-kiss timer set so the arm stores the superflame
  word, four walking at the camera). 216 of 216 calls match. gdb hit counts in the native draw: the tip dropped
  below the first bin (`flame.cpp:154`) 18 times, the odd closing ribbon step (`flame.cpp:300`) 806 times, and the
  superflame word read 1 in seven bursts.
- Flame rotation lerp: `blockBetween` now calls psxport `present::rotationBetween`, which `blendGteControl` also
  uses. The per-field allocation of producer saves and renders is recorded in psxport 0173 with its fix.

- Pacing after a load field. `final4` (load 1.8-2.4) had two more judders after arrival (4098 and 4171, `t = 1`
  presents 4.4 and 1.9 ms late). Cause: psxport `runtime/psx/frame/pace_plan.h:pacePlan` resynced an overrun
  deadline to the moment the field ended, which moved every later deadline 0.457 of a period off the vblank grid,
  so presents a few tenths of a millisecond late before the load now crossed a vblank. Fix: the debt is dropped in
  whole intervals. `final5`: every due time of the run lies on the grid of the first (largest offset 0.0000 periods).

`final5` (final build; load 1.85-2.61 from boot to turn end): playcheck over 8468 intervals p50 16.75 ms, p99
17.00 ms, missed=1. Simulated 60.00 Hz display (vblank half a period after the first present):

| screen | presents | repeats | drops | what it is |
|---|---|---|---|---|
| boot | 57 | 0 | 0 | |
| logos, flag intro, title | 2724 | 0 | 0 | |
| menus | 319 | 0 | 0 | |
| save picker | 615 | 0 | 0 | |
| new game | 12 | 0 | 0 | |
| intro cutscene | 2860 | 0 | 0 | |
| title after cutscene, warp load | 650 | 3 | 0 | 4084 held 4 vblanks before the load field: the still card (0174) |
| `warp 10` turning | 1222 | 0 | 0 | |
| turn end | 9 | 0 | 0 | |

With the vblank a tenth of a period after the due time instead, two presents 2.3 and 2.5 ms late each add a repeat
and a drop: 1825 (save picker) and 3700 (the cutscene's last field).

## Pass 6 (2026-10-21)
Judged as `psxport/docs/workspace/WORKSPACE.md` says: ires=0, fps60=1, the player's aspect (a scratch copy of
`psxport_settings.ini`), `PSXPORT_PRESENT_SINK=3840x2160`, hidden window, no `PSXPORT_NOPACE` (all read back from
`/proc/<pid>/environ`), and a simulated 60.00 Hz display over the `presentseq` times. Pacing is at the guest's
59.94 Hz again (psxport 58862879), so every run has the accepted repeat about every 16.7 s.

- Late presents at 1825 (save picker) and 3700 (the cutscene's last field). Cause: psxport
  `runtime/psx/gpu/ordering_table.cpp:submitOrderingTable` (and the block DMA path) ran a whole DrawOTag, 2-3 ms of
  host work, inside one executor segment, so a present that fell due during it waited for the segment's end. Fix
  (psxport ea8b5f95): the walk and `gpu::dma2Block` show a due present between nodes and blocks.
- Spyro 2 level 21 ran 3-field logic frames where the guest's rule gives 2. Cause:
  `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::runCall/stepFrame` delivered two fields for one
  guest VSync(0), the `FrameBoundary` field and the step's own "frame-tail" field, so every draw that waited a
  host turn became a 3-field frame, and it presented at each wait instead of when the draw returned. Fix: a
  `FrameBoundary` that would reach the step's field bound ends the step with that field, the step presents the
  logic frame's fields when the draw returns (`LoopStop::FrameDrawn`), fields of a frame still running are deferred
  up to `kLongestDeferredFrame` (4, Spyro 1's own delta clamp) and paced as holds past that, and the field redraw
  saves one redraw per in-between of the frame being drawn from the guest's own rule: at least 2 fields, one more
  when the draw's wait flag is set (`FieldOwner::drawnFrameFields`; Spyro 1 0x80075784 read at 0x8001F090, Spyro 2
  0x80067044, Spyro 3 0x8006C678 read at 0x8001E87C).
- Allocation (psxport 0173): the composer, record sealing, frame states, `LevelReads`, `HostMemory`, the OT
  assignments, every Spyro producer save and render and the terrain in-between keep their storage
  (`Session::drawReads`, `renderMemory`, `terrainBins`). A portal save held the kept reads across its call of the
  level sky, whose own save takes them; the reads are now released first. Proof in 0173; guest execution still
  allocates (psxport 0175).

Level scan (`scratch/judder-pass6/scan.sh`, `warp`, settle 6 s, turn 1 s): Spyro 2 levels 10-13, 21-26, 30, 32,
33 and Spyro 3 levels 10-17, 20-26 have no gameplay logic frame other than 1 or 2 fields. Longer frames appear only
on the front end (Spyro 3's menu over the flyby, 3-5 fields; Spyro 2's fade off the title, 3-5 fields) and nothing
composes them there (issue 0207).

| run | screen | presents | repeats | drops | notes |
|---|---|---|---|---|---|
| `judge-s1` (load 1.24-1.70) | boot | 57 | 0 | 0 | |
| | logos, flag intro, title | 2718 | 4 | 1 | 435/437 boot handover (437 2.02 ms late); the rest the 59.94 Hz repeat |
| | menus | 317 | 0 | 0 | |
| | save picker | 615 | 1 | 0 | 59.94 Hz repeat; 1825 no longer late |
| | intro cutscene | 2860 | 4 | 1 | 59.94 Hz repeats; 3700 no longer late |
| | title after cutscene, warp load | 650 | 0 | 0 | the 0174 still card held 3 planned fields |
| | `warp 10` turning | 1220 | 1 | 0 | 59.94 Hz repeat |
| `judge-s2` (load 1.39-1.66) | title, warp, load | 1477 | 5 | 3 | load field 1982 42.67 ms late (psxport 0174); the rest 59.94 Hz |
| | level 21 turning, 66 frames all 2 fields | 131 | 0 | 0 | 66 of 66 in-betweens composed |
| `judge-s3` (load 1.34-1.56) | title, menu, load | 2135 | 8 | 6 | load field 2543 58.65 ms late (0174); the rest 59.94 Hz |
| | level 10 turning, 308 frames all 2 fields | 614 | 1 | 0 | 307 of 307 in-betweens composed; the repeat is 59.94 Hz |

Repeats and drops with the vblank half a period after the first present; with it a tenth of a period after the due
time the only other event is Spyro 1's 4080/4081 in the warp load (3.06 ms late) and Spyro 2's 2305 on arrival
(2.06 ms late). Playcheck: Spyro 1 p50 16.75 ms, p99 17.00 ms, missed 1 of 8458.

## Next
Spyro 2: the opening conversation box is linked by logic before the dispatcher, so the redraw
keeps n+1's box; moving it needs its drawer (FUN_8004D2D8 / FUN_8004A374) re-run at the in-between
in place of logic's packets. Spyro 1's motion trails and demo overlay (issue 0186) show their frames whole
when drawn. Front-end frames of Spyro 2 and 3 compose nothing (0207). Spyro 1's boot handover 437 is 2 ms late.
A real-time check by the operator is still to do: these runs
measure the composed images, not the play feel.
