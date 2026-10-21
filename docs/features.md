# Features — Spyro 1, 2 and 3

Title hops only. Each row is where a Spyro title joins a psxport feature hop (`psxport F-X.n`, psxport
`docs/features.md`); a feature with no row runs on the framework alone. S1 is `SCUS_942.28`, S2
`SCUS_944.25`, S3 `SCUS_944.67`. Paths shorten `titles/spyro1/core/` to `s1/`, `titles/spyro2/` to
`s2/` and `titles/spyro3/` to `s3/`.

## F-BOOT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-BOOT.1 | `game/core/title_runtime_registry.cpp:createCatalog` — `spyro::Catalog`, the three runtimes over the identities generated from each `executable.json` |
| 2 | all | psxport F-BOOT.6 | `game/core/runtime.cpp:spyro::Runtime::createSession` — `spyro::Session`: archive transfer, field owner, HUD draw context, state producers |
| 3 | S1 | psxport F-BOOT.5 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime::startProgram` — `Crt0Boot`, then `spyro1::FrameDriver::initialize` |
| 4 | S1 | psxport F-BOOT.6 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime::registerOverrides` — stock VSync, libgpu, libcd and pad buffers, the projection leaves, the native leaves, the ordering table and screen overlays |
| 5 | S2 | psxport F-BOOT.5 | `titles/spyro2/core/runtime.cpp:spyro2::Runtime::startProgram` — `Crt0Boot`, then `BootPrefixFrameDriver::initialize` |
| 6 | S2 | psxport F-BOOT.6 | `titles/spyro2/core/runtime.cpp:spyro2::Runtime::registerOverrides` — stock VSync, libgpu, `GuestInterruptLibcd`, projection leaves, render overrides, HUD anchor |
| 7 | S3 | psxport F-BOOT.5 | `titles/spyro3/core/runtime.cpp:spyro3::Runtime::startProgram` — as S2 with its own `Crt0Image` |
| 8 | S3 | psxport F-BOOT.6 | `titles/spyro3/core/runtime.cpp:spyro3::Runtime::registerOverrides` — stock VSync, `GuestInterruptLibcd`, projection leaf, render overrides, HUD anchor |

## F-TURN

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-TURN.2 | `titles/spyro1/core/frame/frame_driver.cpp:spyro1::FrameDriver::stepFrame` — boot sequence, then the retail update, the draw and one present |
| 2 | S1 | psxport F-TURN.2 | `titles/spyro1/core/frame/frame_draw.cpp:FrameDraw::run` — GamestateDraw `0x8001ED5C` to its return; each `VSync(0)` of its tail is one field |
| 3 | S1 | psxport F-TURN.2 | `game/core/field_owner.cpp:FieldOwner::presentFrame` — the drawn frame crosses the fence once |
| 4 | S2, S3 | psxport F-TURN.2 | `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::stepFrame` — the retail boot prefix across steps, then update and draw, one presented field per step; a save state carries the suspended call in its `spyro.frame-driver` section |
| 5 | all | title | `game/core/field_owner.cpp:FieldOwner::deliver` — the one delivery sequence of a display field |
| 6 | all | psxport F-TURN.4 | `game/core/runtime.cpp:spyro::Runtime::reportRun` — depth coverage and the producer census |
| 7 | all | psxport F-TURN.5 | `game/core/runtime.cpp:spyro::Runtime::pacePresentation` — fields are already delivered, so presentation only waits |

## F-INPUT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-INPUT.1 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime::deliverPad` — `StockPadBuffers` at `0x800786A0`/`0x800786C2` |
| 2 | S1 | psxport F-INPUT.2 | `titles/spyro1/core/input/input_phase.cpp:InputPhase::of` — which screen takes input, the key of a recorded `.pad` frame |

S2 and S3 take the BIOS pad path.

## F-PICK

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-PICK.1 | `game/core/runtime.cpp:spyro::Runtime::panelLogo` — `extractPanelLogo` over each title's `logoFacts`; only S1 has a wordmark |

## F-CD

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-CD.1 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime` — stock `CdCommand` `0x80064CEC`, `CdControl`/`B`/`F`, `CdSync` `0x800647A0`, work area `0x80074E50` |
| 2 | S1 | title | `game/core/cd_queue.cpp:registerCdQueue` — the cooperative WAD loader leaves over `ArchiveTransfer` |
| 3 | S2 | psxport F-CD.1 | `titles/spyro2/core/runtime.cpp:spyro2::Runtime` — `GuestInterruptLibcd`, ready slot `0x800663B8`, status 2 per stock read |
| 4 | S3 | psxport F-CD.1 | `titles/spyro3/core/runtime.cpp:spyro3::Runtime` — `GuestInterruptLibcd`, ready slot `0x8006B3D0`, status 2 per stock read |
| 5 | S2, S3 | psxport F-CD.3 | `titles/spyro2/core/runtime.cpp:spyro2::Runtime::stockCdReadLanded` (and S3's) — the loader's code modules published as images |

## F-RENDER

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S2 | psxport F-RENDER.2 | `titles/spyro2/core/runtime.cpp:spyro2::Runtime::packetPool` — both parity pools between `0x80069998` and `0x8006B2A4` |
| 2 | S1 | psxport F-RENDER.4 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime::guestPictureIsNativeWidth` — true until the boot logos are done |

## F-WIDE

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-WIDE.1 | `game/core/runtime.cpp:spyro::Runtime::guestWidescreenProjection` — the canvas policy; each runtime binds libgte's projection leaves as `StockLibgte` |
| 2 | S2, S3 | psxport F-WIDE.2 | `game/render/field/guest_moby_visibility.cpp` — the moby walk (`0x80043858`, `0x80030478`) with its plane tests widened by `Game::guestDisplay`'s plan |
| 3 | S2, S3 | psxport F-WIDE.2 | `game/render/terrain/guest_terrain_drawer.cpp` — the terrain drawer with its X outcodes on the widened window |
| 4 | S2, S3 | title | `titles/spyro2/render/hud_anchor.cpp`, `titles/spyro3/render/hud_anchor.cpp` — edge HUD widgets through `spyro::ui_anchor` |
| 5 | S1 | psxport F-WIDE.2 | `titles/spyro1/render/terrain_sectors.cpp` — RenderWorldChunks' sector plane on `presentedSlope`, its sector marks (`0x800771C8`) and animation on retail's; its face clip on the widened window through the shared passes |
| 6 | S1 | psxport F-WIDE.2 | `titles/spyro1/render/actor_list.cpp`, `titles/spyro1/render/sprite_queue.cpp` — the moby list planes (`0x8001F158`, `0x800208FC`) on `presentedSlope`; the sprite queue (`0x80022A2C`) culled and outcoded on the widened window; both set the drawn byte +0x51, which level code reads, on retail's plane |
| 7 | S1 | title | `titles/spyro1/render/hud_anchor.cpp` — HUD records in the sprite queue and the life-orb and egg quads (`0x8001919C`) through `spyro::ui_anchor` |
| 8 | S1 | psxport F-WIDE.2 | `titles/spyro1/render/sky.cpp`, `titles/spyro1/render/glows.cpp`, `titles/spyro1/render/particles.cpp` — the cyclorama (`0x8004EBA8`), glows (`0x800580F4`, `0x800584C4`) and particles (`0x800573C8`) test x on the widened window; the seen and fade bytes they write follow `titles/spyro1/render/screen.h:kRetailBounds` |
| 9 | S1 | psxport F-WIDE.2 | `titles/spyro1/render/actor_emitter_vertices.cpp:ActorEmitter::keepScreen` — the moby emitters (`0x8001F798`, `0x80020F34`) outcode a straddling moby's vertices on `spyro::presentedBounds` (`guest_view_slope.h`) |

The portal bodies of `0x80050BD0` clip at 512 (issue 0186).

## F-FPS60

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-FPS60.1 | `game/core/runtime.cpp:spyro::Runtime::stateProducer` — `spyro::Session::stateProducers` |
| 2 | S2, S3 | psxport F-FPS60.1 | `game/render/terrain/terrain_state_producer.cpp:TerrainStateProducer` — the terrain drawer (`0x80023BB4`, `0x80022378`) redrawn over the field's saved words at t |
| 3 | S2, S3 | psxport F-FPS60.1 | `game/render/engine_render_overrides.h:engine_render::flatten` — every packet's bin through `depth_bins::assignBins`, from the flatten (`0x8001B2A8`, `0x8004E664`) |
| 4 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/screen_overlays.cpp:FullscreenTint`, `CinematicLetterbox` — `0x800190D4` and `0x80018F30` |
| 5 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/ordering_table.cpp:OrderingTable` — LinkOTPrimitives `0x80016784` gives each linked packet its slot |
| 7 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/terrain_image.cpp:TerrainImage` — RenderWorldChunks `0x800258F0` redrawn over the field's saved words at t, camera from `CameraBuilder` (`0x80033C50`) |
| 8 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/sprite_queue_producer.cpp:SpriteQueueProducer`, `titles/spyro1/render/paired_actor_producer.cpp:PairedActorProducer` — each queued record (`0x80022A2C`) and Spyro (`0x80023AC4`) redrawn at t, position and angles moved |
| 9 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/sky_producer.cpp:SkyProducer`, `titles/spyro1/render/drop_shadow_producer.cpp:DropShadowProducer`, `titles/spyro1/render/glow_producers.cpp:GlowFanProducer`, `SparkleProducer`, `titles/spyro1/render/particle_producer.cpp:ParticleProducer` — the sky (`0x8004EBA8`), drop shadow (`0x80059A48`), each glow emitter, sparkle and particle record redrawn at t |
| 10 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/actor_emitter_producer.cpp:ActorEmitterProducer` — each moby the emitters (`0x8001F798`, `0x80020F34`) draw, its list entry rebuilt and drawn at t, record and camera moved |
| 6 | all | psxport F-FPS60.2 | `game/core/frame_cut.cpp:FrameCut::isCut` — game state, level, or (S2, S3) a camera placement inside a scene |

S1's portal bodies and remaining effects have no producer yet; their in-betweens repeat the shown record (issue 0186).

## F-DEBUG

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-DEBUG.1 | `titles/spyro1/core/runtime/runtime.cpp:spyro1::Runtime::titleCommand` — `gate_debug::command`: `gates`, `gate-teleport` |
| 2 | S1/S2/S3 | title | `game/core/runtime.cpp:spyro::Runtime::titleCommand` — `warp <level-id>` arms `game/core/debug/warp.cpp:spyro::Warp`; the S1 `FrameDriver` and S2/S3 `BootPrefixFrameDriver` apply it before the update once the title screen is idle, through `titles/spyro1/core/debug/warp.cpp`, `titles/spyro{2,3}/core/warp.cpp` |
