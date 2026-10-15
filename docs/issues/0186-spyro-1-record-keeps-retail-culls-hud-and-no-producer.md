---
id: 186
title: Spyro 1 on Record keeps retail culls and HUD placement and has no world, actor or sky producer
status: open
symptom: at 16:9 the margins show only geometry that straddles the 4:3 edge and the HUD stays at its 4:3 columns; at 60 fps every in-between repeats the shown record except the fullscreen tint and letterbox
tags: spyro1,render,record,widescreen,fps60
features: [F-FPS60, F-WIDE]
created: 2026-10-15
---

## Observed
Since Spyro 1 moved to the Record path, GamestateDraw `0x8001ED5C` and everything it calls run
unmodified, so the device draws retail's ordering table. recordcheck at t=1 matches (S005). The
canvas widens through `SpyroRuntime::guestWidescreenProjection`, but nothing else is widened,
anchored or interpolated.

## Why nothing is overridden yet
Every Spyro 1 horizontal cull and HUD placement sits inside a whole guest routine with no
`jal`-reached leaf at the decision, so a native override cannot take just the decision:

| what | guest routine | where the decision is |
|---|---|---|
| world sector plane, face clip | RenderWorldChunks `0x800258F0` (0x4E08 bytes) | inline |
| moby plane, regular pass | BuildActorDrawList `0x8001F158` (hand asm, static register save area) | inline |
| moby plane, secondary pass | `0x800208FC` | inline |
| shadow staging, shaded pass; HUD mobys | RasterizeSpritePrimQueue `0x80022A2C` | inline; per-moby OFX via `ctc2` at `0x80022D2C`/`0x8002395C` |
| particles, glows, sky | `0x800573C8`, `0x80058BA8`, `0x80050BD0` | inline |
| HUD sprites | EnqueueLoadingScreenSprites `0x80019300` into the sprite queue | drawn by `0x80022A2C` |

RenderWorldChunks is not the Spyro 2/3 terrain drawer (`0x80023BB4`/`0x80022378`), so the shared
`guest_terrain::TerrainStateProducer` does not apply to it.

## Producers so far
The fullscreen tint `0x800190D4` and the cinematic letterbox `0x80018F30` are state producers
(`spyro1::ScreenOverlays`), and the LinkOTPrimitives override `0x80016784` gives every packet its
slot (`spyro1::OrderingTable`). The native renderer under `game/render/` (FrameRenderer, the
RenderQueue producers, the temporal layer) was deleted: it emitted HQ/RenderQueue geometry, not the
guest's packets, so none of it was a producer to rebind.

## Proper fix
The Spyro 2 shape: native overrides of these routines that write the guest's own packets into its
pool and ordering table, widen only the drawn answer, and register a `StateProducer` per family.
Terrain comes first, because a world-space producer drawn against retail terrain judders
relative to it. RenderWorldChunks shares three quarters of its body with Spyro 2's drawer (see
`docs/re-frontier.md` `spyro1.record`), so the port is the shared `guest_terrain` passes plus a
Spyro 1 sector loop, flag masks and its own block, held exact by the override differential; the
camera builder `0x80033C50` needs its own composition order and rounding before
`guest_camera::Builder` can rebuild an in-between. Then the paired actor `0x80023AC4`, the drop
shadow `0x80059A48` and the routines in the table above, each a byte-exact port of its body.
