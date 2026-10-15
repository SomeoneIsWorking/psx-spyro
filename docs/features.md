# Features — Spyro 1, 2 and 3

Title hops only. Each row is where a Spyro title joins a psxport feature hop (`psxport F-X.n`, psxport
`docs/features.md`); a feature with no row runs on the framework alone. S1 is `SCUS_942.28`, S2
`SCUS_944.25`, S3 `SCUS_944.67`. Paths shorten `titles/spyro1/core/` to `s1/`, `titles/spyro2/` to
`s2/` and `titles/spyro3/` to `s3/`.

## F-BOOT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-BOOT.1 | `game/core/title_runtime_registry.cpp:createCatalog` — `SpyroCatalog`, the three runtimes over the identities generated from each `executable.json` |
| 2 | all | psxport F-BOOT.6 | `game/core/spyro_runtime.cpp:SpyroRuntime::createSession` — `SpyroSession`: archive transfer, field owner, HUD draw context, state producers |
| 3 | S1 | psxport F-BOOT.5 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime::startProgram` — `Crt0Boot`, then `Spyro1FrameDriver::initialize` |
| 4 | S1 | psxport F-BOOT.6 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime::registerOverrides` — stock VSync, libgpu, libcd and pad buffers, the projection leaves, the native leaves, the ordering table and screen overlays |
| 5 | S2 | psxport F-BOOT.5 | `s2/core/spyro2_runtime.cpp:Spyro2Runtime::startProgram` — `Crt0Boot`, then `BootPrefixFrameDriver::initialize` |
| 6 | S2 | psxport F-BOOT.6 | `s2/core/spyro2_runtime.cpp:Spyro2Runtime::registerOverrides` — stock VSync, libgpu, `GuestInterruptLibcd`, projection leaves, render overrides, HUD anchor |
| 7 | S3 | psxport F-BOOT.5 | `s3/core/spyro3_runtime.cpp:Spyro3Runtime::startProgram` — as S2 with its own `Crt0Image` |
| 8 | S3 | psxport F-BOOT.6 | `s3/core/spyro3_runtime.cpp:Spyro3Runtime::registerOverrides` — stock VSync, `GuestInterruptLibcd`, projection leaf, render overrides, HUD anchor |

## F-TURN

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-TURN.2 | `s1/frame/spyro1_frame_driver.cpp:Spyro1FrameDriver::stepFrame` — boot sequence, then the retail update, the draw and one present |
| 2 | S1 | psxport F-TURN.2 | `s1/frame/spyro1_frame_draw.cpp:FrameDraw::run` — GamestateDraw `0x8001ED5C` to its return; each `VSync(0)` of its tail is one field |
| 3 | S1 | psxport F-TURN.2 | `s1/runtime/spyro1_field_scheduler.cpp:FieldScheduler::presentFrame` — the drawn frame crosses the fence once |
| 4 | S2, S3 | psxport F-TURN.2 | `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::stepFrame` — the retail boot prefix across steps, then update and draw, one presented field per step |
| 5 | all | title | `game/core/field_owner.cpp:FieldOwner::deliver` — the one delivery sequence of a display field |
| 6 | all | psxport F-TURN.4 | `game/core/spyro_runtime.cpp:SpyroRuntime::reportRun` — depth coverage and the producer census |
| 7 | all | psxport F-TURN.5 | `game/core/spyro_runtime.cpp:SpyroRuntime::pacePresentation` — fields are already delivered, so presentation only waits |

## F-INPUT

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-INPUT.1 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime::deliverPad` — `StockPadBuffers` at `0x800786A0`/`0x800786C2` |
| 2 | S1 | psxport F-INPUT.2 | `s1/input/spyro1_input_phase.cpp:InputPhase::of` — which screen takes input, the key of a recorded `.pad` frame |

S2 and S3 take the BIOS pad path.

## F-PICK

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-PICK.1 | `game/core/spyro_runtime.cpp:SpyroRuntime::panelLogo` — `extractPanelLogo` over each title's `logoFacts`; only S1 has a wordmark |

## F-CD

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-CD.1 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime` — stock `CdCommand` `0x80064CEC`, `CdControl`/`B`/`F`, `CdSync` `0x800647A0`, work area `0x80074E50` |
| 2 | S1 | title | `game/core/cd_queue.cpp:registerCdQueue` — the cooperative WAD loader leaves over `ArchiveTransfer` |
| 3 | S2 | psxport F-CD.1 | `s2/core/spyro2_runtime.cpp:Spyro2Runtime` — `GuestInterruptLibcd`, ready slot `0x800663B8`, status 2 per stock read |
| 4 | S3 | psxport F-CD.1 | `s3/core/spyro3_runtime.cpp:Spyro3Runtime` — `GuestInterruptLibcd`, ready slot `0x8006B3D0`, status 2 per stock read |
| 5 | S2, S3 | psxport F-CD.3 | `s2/core/spyro2_runtime.cpp:Spyro2Runtime::stockCdReadLanded` (and S3's) — the loader's code modules published as images |

## F-RENDER

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-RENDER.1 | `Spyro{1,2,3}Runtime::renderCapabilities` — Record, guest VRAM is the picture |
| 2 | S2 | psxport F-RENDER.2 | `s2/core/spyro2_runtime.cpp:Spyro2Runtime::packetPool` — both parity pools between `0x80069998` and `0x8006B2A4` |
| 3 | S1 | psxport F-RENDER.4 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime::guestPictureIsNativeWidth` — true until the boot logos are done |

## F-WIDE

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-WIDE.1 | `game/core/spyro_runtime.cpp:SpyroRuntime::guestWidescreenProjection` — the canvas policy; each runtime binds libgte's projection leaves as `StockLibgte` |
| 2 | S2, S3 | psxport F-WIDE.2 | `game/render/field/guest_moby_visibility.cpp` — the moby walk (`0x80043858`, `0x80030478`) with its plane tests widened by `Game::guestDisplay`'s plan |
| 3 | S2, S3 | psxport F-WIDE.2 | `game/render/terrain/guest_terrain_drawer.cpp` — the terrain drawer with its X outcodes on the widened window |
| 4 | S2, S3 | title | `s2/render/spyro2_hud_anchor.cpp`, `s3/render/spyro3_hud_anchor.cpp` — edge HUD widgets through `spyro::ui_anchor` |

S1 keeps retail's culls and HUD placement (issue 0186).

## F-FPS60

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-FPS60.1 | `game/core/spyro_runtime.cpp:SpyroRuntime::stateProducer` — `SpyroSession::stateProducers` |
| 2 | S2, S3 | psxport F-FPS60.1 | `game/render/terrain/terrain_state_producer.cpp:TerrainStateProducer` — the terrain drawer (`0x80023BB4`, `0x80022378`) redrawn over the field's saved words at t |
| 3 | S2, S3 | psxport F-FPS60.1 | `game/render/engine_render_overrides.h:engine_render::flatten` — every packet's bin through `depth_bins::assignBins`, from the flatten (`0x8001B2A8`, `0x8004E664`) |
| 4 | S1 | psxport F-FPS60.1 | `s1/hud/screen_overlays.cpp:FullscreenTint`, `CinematicLetterbox` — `0x800190D4` and `0x80018F30` |
| 5 | S1 | psxport F-FPS60.1 | `s1/frame/spyro1_ordering_table.cpp:OrderingTable` — LinkOTPrimitives `0x80016784` gives each linked packet its slot |
| 6 | all | psxport F-FPS60.2 | `game/core/frame_cut.cpp:FrameCut::isCut` — game state, level, or (S2, S3) a camera placement inside a scene |

S1's world, actors and sky have no producer yet; their in-betweens repeat the shown record (issue 0186).

## F-DEBUG

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-DEBUG.1 | `s1/runtime/spyro1_runtime.cpp:Spyro1Runtime::titleCommand` — `gate_debug::command`: `gates`, `gate-teleport` |
| 2 | S1 | title | `s1/frame/stage_update_observer.cpp:StageUpdateObserver`, `s1/execution/handoff_store_observer.cpp:HandoffStoreObserver` — opt-in samples on the `stage-observe` and `handoff-store` channels |
