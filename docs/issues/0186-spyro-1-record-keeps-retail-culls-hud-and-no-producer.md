---
id: 186
title: Spyro 1 on Record keeps retail culls and has no producer for its motion trails
status: open
symptom: at 16:9 portals fill only to the 4:3 edge; at 60 fps a present holding a motion trail shows the field whole
tags: spyro1,render,record,widescreen,fps60
features: [F-FPS60, F-WIDE]
created: 2026-10-15
---

## Observed
Since Spyro 1 moved to the Record path, GamestateDraw `0x8001ED5C` and everything it calls run
unmodified, so the device draws retail's ordering table. recordcheck at t=1 matches (S005). The
canvas widens through `spyro::Runtime::guestWidescreenProjection`, but nothing else is widened,
anchored or interpolated.

## Where each decision is
Every Spyro 1 horizontal cull and HUD placement sits inside a whole guest routine with no
`jal`-reached leaf at the decision, so each is a byte-exact native port of the routine:

| what | guest routine | state |
|---|---|---|
| moby plane, regular pass | BuildActorDrawList `0x8001F158` (hand asm, static register save area) | native, plane on `presentedSlope` (`spyro1::actors::buildList`, `kPrimaryListPass`) |
| moby plane, secondary pass | `0x800208FC` | native, widened the same way (`kSecondaryListPass`) |
| shadow staging, lit and HUD mobys | RasterizeSpritePrimQueue `0x80022A2C` | native, widened, producer (`spyro1::actors::SpriteQueue`) |
| Spyro | RasterizePairedActor `0x80023AC4` | native, producer (`spyro1::actors::PairedActor`); its 45-degree cull stays retail's |
| HUD icons and text | EnqueueLoadingScreenSprites `0x80019300` into the sprite queue | anchored by class in the queue (`spyro1::hud_anchor`) |
| HUD life orbs and egg row | EmitTexturedQuad `0x8001919C` from `0x800195C4`, `0x8001965C` | anchored by return address (`spyro1-hud-quad`) |
| moby polygons, regular | EmitActorDrawList `0x8001F798` (hand asm) | native, x outcodes on the presented window, producer per drawn record (`spyro1::actors::ActorEmitter`) |
| moby polygons, secondary and lit | EmitSecondaryActorPrimitives `0x80020F34` (hand asm) | native the same way (`spyro1::actors::LitActorEmitter`) |
| drop shadow | DrawSpyroDropShadow `0x80059A48` | native, producer keyed by the shadow block (`spyro1::shadow::DropShadow`); no horizontal cull |
| particles | RasterizeEmitList `0x800573C8` (hand asm) | native, x tests widened, producer per drawn record (`spyro1::particles::EmitList`) |
| glows | `0x80058BA8` (C) calls the fans `0x800580F4` and sparkles `0x800584C4` | native, x outcodes widened, producer per emitter and per sparkle (`spyro1::glows`) |
| sky | cyclorama `0x8004EBA8`, the tail of `0x80050BD0` | native, x outcodes widened, producer (`spyro1::sky::Cyclorama`) |
| portals | `0x80050BD0`: fill `0x8004FEA0`, near sky `0x8004F4BC` / `0x80050240` | native, producer per portal (`spyro1::portals::Portals`); fill and window on retail's 512x240 |
| moby shadows | RenderMobyShadows `0x80059F8C` | native, x tests widened, producer per record (`spyro1::shadows::MobyShadows`) |
| flame (horn strike) | `0x80058D64` | native, producer (`spyro1::flame`) |
| dragon-rescue star | `0x80058864` | native, producer (`spyro1::rescue_star::RescueStarProducer`) |
| demo overlay | `0x80018908` | guest; it queues DEMO MODE as HUD mobys and draws nothing itself, so the sprite queue producer covers it |
| motion trails | `0x800189F0` | guest, no producer |

A widened draw leaves guest state as retail's 4:3 window writes it: a particle's seen byte and a
sparkle's fade follow `spyro1::kRetailBounds`, so a sparkle in a margin is drawn once and dies as
retail's would; the list builders and sprite queue set a moby's drawn byte +0x51 (read by
BuildRenderEntityLists `0x800521C0`, `0x80051FEC` and level AI) on retail's plane, and the sector
loop marks `0x800771C8` and animates a sector only when retail's sphere test keeps it. A turned particle behind its depth bias leaves the GTE rotation as its facing scale
for the records after it; an in-between draws each record from the camera, so those records can
differ from the field's at t=1.

## Producers so far
The fullscreen tint `0x800190D4` and the cinematic letterbox `0x80018F30` (`spyro1::ScreenOverlays`),
the terrain RenderWorldChunks `0x800258F0` (`spyro1::terrain::TerrainImage`), the sprite queue
`0x80022A2C` (one save per queued record, keyed by the record), the flame `0x80058D64`, the rescue star `0x80058864`, Spyro `0x80023AC4` (keyed by his
record), the moby emitters `0x8001F798` and `0x80020F34` (one save per drawn record), the sky `0x8004EBA8`, the drop shadow `0x80059A48`, each glow emitter `0x800580F4` and
sparkle `0x800584C4`, each particle record `0x800573C8`, each portal `0x80050BD0` and each moby shadow `0x80059F8C`; the
LinkOTPrimitives override `0x80016784` gives every packet its slot (`spyro1::OrderingTable`).
See `docs/re-frontier.md` `spyro1.record`.

Census, attract route, shipping settings, 6000 steps: 7823787 primitives, 7787825 keyed and 35962
unkeyed (before the moby emitters: 276457 unkeyed; before the effects: 3982144 of 7269141). The census names the nearest
`jalr`-reached function, so every unkeyed primitive shows as GamestateDraw `0x8001ED5C`; they are
its `jal` callees in the table's last two rows.

## Portals and moby shadows (2026-10-21)
Cause: `0x80050BD0` and `0x80059F8C` ran as guest code, so their packets carried no key; any
present holding one (every gameplay frame with a portal or a shadowed moby in view) showed frame n
whole.

Fix: both are native ports that run over guest memory on the field and over host memory for an
in-between. `spyro1::portals::PortalProducer` saves, per portal under its own emission scope, the
camera, the near sky's spin and the level clock; its render redraws the fill, and within 0x4000 the
near sky, at t. The near sky is the cyclorama's sector walk (`sky::Cyclorama::drawThrough`) with
each triangle cut to the outline by `Portals::cutShaded` (F7E8) and the faded colours from the
fog owner's `fadeColours`; its rotations come from the camera builder `0x80033C50` with the spin
applied (`nearSkyRotations`). `spyro1::shadows::MobyShadowProducer` keys each staged record.

Proof (fps60, 16:9, hidden window, `PSXPORT_PRODUCERS=1`): on `warp` 10, 11 and 20 and beside
level 10's portal (`gate-teleport 0 0`, near sky drawn), the census after gameplay starts adds only
keyed primitives with a render (the 17 unkeyed are the level-entry transition, before gameplay);
40 presents while turning have even adjacent differences and a steady image shift; real frames are
pixel-identical to the guest-drawn run while no shadowed moby sits in a side margin, where the
native shadow cull on the widened window draws what retail's 512-wide test culled. A temporary in-process differential (removed) matched the
near sky's packets against `0x8004F4BC` beside all five of level 10's gates, and its packets and
faded table against `0x80050240` with a forced fade beside two of them. Captures: `scratch/interp/final{10,11,20,near,hud}/`.

## Shared owners (2026-10-21)
The moby record layout and blend (`game/render/field/moby_record.*`, `record_blend.h`) and the
overlay pair (`guest_render_globals::Globals::overlayPair`, `depth_bins::overlayBins`) moved to
`game/` for Spyro 2's field producer; Spyro 1's actor, shadow, particle, glow and sprite-queue
producers use them. Spyro 1 `warp` 10 at fps60 16:9 afterwards: no unkeyed primitive after gameplay
starts, adjacent differences 7.1..10.4 (`scratch/interp2/s1`).

## Dragon rescue (2026-10-22)
Reproduction: player settings (16:9, ires auto, fps60), `PSXPORT_PRESENT_SINK=3840x2160`, hidden window,
`warp 10`, then `dragon-teleport 0` on the control channel: Spyro is placed on the level's first crystal
dragon (moby class 250) and the statue's own touch calls `func_8002C924` into gamestate 8 within a frame.
The first proof below walked into the statue by holding a button, which steers gameplay; it was redone
on this route (review, below).

Before: 34 of 38 in-betweens in gamestate 8 were shown whole (`compose` names unkeyed packets from
GamestateDraw), and a stepped capture showed the white flash popping on and off at the in-between
(its difference to its neighbours 66 against 4.3).

Cause, star: `0x80058864` ran as guest code with no emission scope, so the dragon draw `0x8001CFDC`
linked unkeyed packets and `FrameComposer::compose` showed those frames whole. Cause, flash: the
fullscreen tint `0x800190D4` is skipped by the guest at fade 0, and `psxport frame_composer.cpp:
FrameComposer::compose` drew an object only one of two frames held at that frame's state, so the
additive flash appeared and vanished whole.

Fix: `spyro1::rescue_star` ports `0x80058864` (centre through the camera into TR, the block's RT
rotation, an inner ring at scale/4096 and spikes at scale/1024 staged in the scratchpad, an additive
draw mode and sixteen flat triangles appended to the overlay pair) and saves the camera and the
0x2C-byte block at `0x80076248`; the render moves position, scale and grey linearly and the rotation
through `rotationBetween`, under the in-between camera. psxport `StateProducer::rests` names a skipped
draw as a state; `FullscreenTint::rests` answers an additive or subtractive tint at colour 0, so the
flash fades in and out across the in-between.

Proof: override differential, 16 of 16 sampled calls match (memory, $v0, $v1). Real speed, gamestate 8:
320 of 320 in-betweens composed. Stepped 8x capture (`scratch/inbetween-gaps/s1-star8xb/star-and-flash.png`):
each in-between's difference to the frame before over the one after is 0.87..1.44 across the star and
the flash start (1.04), against 66/4.3 before; at 1x the flash's vanish is 33.5/33.4 and its start 24.4/28.6.

Review (2026-10-22): the stepped captures of `s1-star8xb` were deleted, so the figures above could not
be rechecked. Redone on the `dragon-teleport` route at HEAD: `scratch/inbetween-gaps/fx-s1rescue2`
(8x, 40 presents from gamestate 8's first frame): the star grows and the additive flash ramps the mean
grey 99 -> 208 over 14 presents with each present's difference to the one before about equal to the one
after (16.3/15.7, 15.7/15.3, ... 5.6/3.5, against 28.9..8.6 between its neighbours), so every in-between
of the star and the flash sits halfway. Real speed, no capture (`fx-s1`, load 1.46-1.89): gamestate 8
696 of 696 in-betweens composed, simulated 60.00 Hz display 2 repeats, 0 drops.

## Motion trails (2026-10-22)
`0x800189F0` draws up to four trails pushed each frame (count `0x80075684`, cleared by the draw; point
counts `0x800772C8`, lists `0x80078658`). A trail is a list of 0x1C-byte points (world position, its
projection at +0xC, depth +0x14, age +0x18) drawn as Gouraud ribbons between consecutive points, faded
by age. The only pusher on the disc is moby type 104 in the level code of WAD entries 57 and 75
(levels 50 and 63; it appends at the list end, at most 0x78 points, and ages every point by the frame
delta, so a point keeps its index). No instance of type 104 is loaded with either level, and 600
frames idle on each pushed none; what spawns it is not found, so no trail has been drawn under the port.

## Proper fix
The motion trails: a producer pairing points by index, once the spawner of moby type 104 is found
and a trail can be reached.
The portal's 16:9 width is issue 0211.
