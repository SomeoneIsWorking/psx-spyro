---
id: 186
title: Spyro 1 on Record keeps retail culls and has no producer for its trails and demo overlay
status: open
symptom: at 16:9 portals fill only to the 4:3 edge; at 60 fps a present holding a horn or motion trail or the demo overlay shows the field whole
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
| other effects | horn trails `0x80058D64`, motion trails `0x800189F0`, demo overlay `0x80018908`, `0x80058864` | guest, no producer |

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
`0x80022A2C` (one save per queued record, keyed by the record), Spyro `0x80023AC4` (keyed by his
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

## Proper fix
The same shape for what is left: the horn and motion trails, the demo overlay and `0x80058864`.
For 16:9, the portal fill's screen triangles (`0x8006CB8C`, retail 512x240) and the outline window
clamp (`Portals::publishWindow`) need the presented width.
