---
id: 186
title: Spyro 1 on Record keeps retail culls and has no producer for its portals and remaining effects
status: open
symptom: at 16:9 portals clip at the 4:3 edge; at 60 fps in-betweens repeat the portals, spikes, horn and motion trails and the demo overlay
tags: spyro1,render,record,widescreen,fps60
features: [F-FPS60, F-WIDE]
created: 2026-10-15
---

## Observed
Since Spyro 1 moved to the Record path, GamestateDraw `0x8001ED5C` and everything it calls run
unmodified, so the device draws retail's ordering table. recordcheck at t=1 matches (S005). The
canvas widens through `SpyroRuntime::guestWidescreenProjection`, but nothing else is widened,
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
| portals | `0x80050BD0`'s bodies `0x8004F4BC`, `0x8004FEA0`, `0x80050240` | guest, 512/240 clips, no producer |
| other effects | spikes `0x80059F8C`, horn trails `0x80058D64`, motion trails `0x800189F0`, demo overlay `0x80018908`, `0x80058864` | guest, no producer |

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
sparkle `0x800584C4`, and each particle record `0x800573C8`; the LinkOTPrimitives override
`0x80016784` gives every packet its slot (`spyro1::OrderingTable`). Tests:
`tests/test_spyro1_terrain.cpp`, `tests/test_spyro1_actors.cpp`, `tests/test_spyro1_effects.cpp`;
the margin tests there check +0x51 and the sector marks at 16:9.
See `docs/re-frontier.md` `spyro1.record`.

Census, attract route, shipping settings, 6000 steps: 7823787 primitives, 7787825 keyed and 35962
unkeyed (before the moby emitters: 276457 unkeyed; before the effects: 3982144 of 7269141). The census names the nearest
`jalr`-reached function, so every unkeyed primitive shows as GamestateDraw `0x8001ED5C`; they are
its `jal` callees in the table's last two rows.

## Proper fix
The same shape for what is left: the portal bodies and the remaining effects in the table.
