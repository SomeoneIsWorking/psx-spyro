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
| 4 | S2, S3 | psxport F-TURN.2 | `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::stepFrame` — the retail boot prefix across steps, then update and draw; a guest VSync(0) delivers one field, and a logic frame's fields are presented when its draw returns, its fields deferred up to the title's delta clamp (`BootPrefixFacts::deltaClampFields`), and each later field of a stall presented; a save state carries the suspended call in its `spyro.frame-driver` section |
| 5 | all | title | `game/core/field_owner.cpp:FieldOwner::deliver` — the one delivery sequence of a display field |
| 6 | all | psxport F-TURN.4 | `game/core/runtime.cpp:spyro::Runtime::reportRun` — depth coverage and the producer census |
| 7 | all | psxport F-TURN.5 | `game/core/runtime.cpp:spyro::Runtime::presentDeadline` — fields are already delivered, so presentation only plans its deadline |

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
| 10 | S1 | psxport F-WIDE.2 | `titles/spyro1/render/moby_shadows.cpp:MobyShadows::draw` — the moby shadows (`0x80059F8C`) cull the shadow centre on `spyro::presentedBounds` instead of retail's 512-wide window (`8005A330..8005A354`), so a shadow in a side margin is drawn |

The portal fill and near sky of `0x80050BD0` stay on retail's 512-wide window: the fill covers the screen triangles at `0x8006CB8C` and the outline window is clamped to [0, 512] (issue 0186).

## F-FPS60

| hop | title | joins | owner |
|---|---|---|---|
| 1 | all | psxport F-FPS60.1 | `game/core/runtime.cpp:spyro::Runtime::stateProducer` — `spyro::Session::stateProducers` |
| 2 | S2, S3 | psxport F-FPS60.1 | `game/core/draw_section.cpp:spyro::DrawSection`, `titles/spyro2|3/core/draw_section.cpp`, `game/core/draw_motion.cpp:DrawMotion` — the per-field draw body (`0x800156FC`, `0x8001E638`) replayed for each in-between, its state switch whole and its device tail skipped, with the camera, Spyro, his poses and shadow, every moby slot and the effects pools written k/n of the way; the loading screen (state 5) is held (issue 0237) |
| 3 | S2, S3 | psxport F-FPS60.1 | `game/render/engine_render_overrides.h:engine_render::flatten` — every packet's bin through `depth_bins::assignBins`, from the flatten (`0x8001B2A8`, `0x8004E664`) |
| 4 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/screen_overlays.cpp:FullscreenTint`, `CinematicLetterbox` — `0x800190D4` and `0x80018F30` |
| 5 | S1 | psxport F-FPS60.1 | `titles/spyro1/render/ordering_table.cpp:OrderingTable` — LinkOTPrimitives `0x80016784` gives each linked packet its slot |
| 7 | S1 | psxport F-FPS60.1 | `titles/spyro1/core/frame/draw_section.cpp:DrawSection`, `draw_motion.cpp:DrawMotion` — GamestateDraw's GS_Playing section (`0x8001ED5C`) and the title's TitlescreenDraw (`0x8007CEE4`, attract, menu, save picker) replayed for each in-between with its motion tracks written k/n of the way (camera with sub-unit angles rebuilt into both matrices, Spyro, mobys, HUD, particles, sparkles, flame, title slides); other states are held |
| 6 | all | psxport F-FPS60.2 | `game/core/frame_cut.cpp:FrameCut::isCut` — game state, level, or (S2, S3) a camera placement inside a scene, or (S2) a cutscene track's shot change by the track player's own blend test |

S1's motion trails `0x800189F0` have no producer (unreached: only moby type 104 in levels 50 and 63 pushes them; issue 0186). S2 and S3 have no producers left but the shared fullscreen tint and letterbox, whose renders only run at t = 1; their fades, letterbox and conversation slide step at 30 fps until they are tracks (issue 0237).

## F-DEBUG

| hop | title | joins | owner |
|---|---|---|---|
| 1 | S1 | psxport F-DEBUG.1 | `titles/spyro1/core/debug/debug_menu.cpp:spyro1::DebugMenu` — the `portal` and `dragon <n>` arrivals and the `gates`, `dragons` listings over `titles/spyro1/core/debug/teleport.cpp` |
| 2 | S1/S2/S3 | title | `game/core/debug/debug_menu.cpp:spyro::DebugMenu` — `warp <homeworld>/<level>/<arrival>` (and `mobys`/`moby-teleport` in S1/S2) arms `game/core/debug/warp.cpp:spyro::Warp`; the S1 `FrameDriver` and S2/S3 `BootPrefixFrameDriver` apply it before the update once the title screen is idle, through `titles/spyro1/core/debug/warp.cpp`, `titles/spyro{2,3}/core/warp.cpp`; in S1 also from GS_Playing with no load running, through the level-warp cheat's own transition `CheatProcessLevelWarp` `0x8002D580` (`spyro1::Warp::changeLevel`), so a start, portal or dragon in another level arrives from play; S2/S3 still wait for the title screen (issue 0227) |
