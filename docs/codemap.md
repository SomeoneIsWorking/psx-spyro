# Codemap — Spyro 1, 2 and 3

Where the code lives and what each owner owns. Feature chains are in `docs/features.md`, goals in
`docs/project-goals.md`, state in `docs/project-state.md`, binary evidence in `docs/re-frontier.md`;
the framework side is `external/psxport/docs/codemap.md`. S1 is `SCUS_942.28`, S2 `SCUS_944.25`,
S3 `SCUS_944.67`. `game/` and `titles/` are the include roots, so a header is included through its directory:
`"core/runtime.h"` under `game/`, `"spyro1/render/sky.h"` under `titles/`.

## Owners

### Shared engine — `game/core/` (`spyro`)

| owner | path | owns |
|---|---|---|
| `spyro::Catalog`, `createCatalog` | `game/core/title_runtime_registry.h` | The product catalog: the three runtimes and their generated identities, provisioned under `scratch/assets` |
| `spyro::Runtime` | `game/core/runtime.h` | The `GameRuntime` base the three titles share: session, state producers, presentation pacing, the widescreen projection policy, panel logo, run report |
| `spyro::Session`, `session` | `game/core/session.h` | Per-Game title state: archive transfer, field owner, HUD draw context, terrain visibility and budget, state producers |
| `FieldOwner`, `FieldObserver` | `game/core/field_owner.h` | The one delivery sequence of a display field: pad, clock, VBlank root, observers, snapshot, fence, audio, host turn |
| `BootPrefixFrameDriver`, `BootPrefixFacts` | `game/core/boot_prefix_frame_driver.h` | S2/S3's `FrameDriver`: the retail boot prefix as a resumable call, then update and draw, one presented field per step; owns the title's `FrameCut` |
| `FrameCut`, `FrameCutFacts` | `game/core/frame_cut.h` | Whether a sealed record is a cut: game state, level, or (S2/S3) a camera placement inside a scene; sampled by each frame driver |
| `GuestExecution`, `WorldGuestExecution`, `GuestFrameScope` | `game/core/guest_execution.h`, `game/core/world_guest_execution.h`, `game/core/native_execution.h` | Root guest continuation across budget yields, resumed retail world work, the `$ra` of a nested guest call |
| `ArchiveTransfer`, `archive_transfer::decide` | `game/core/archive_transfer.h`, `game/core/archive_transfer_contract.h` | Bounded atomic WAD transfer with per-Core completion; the refusal decision |
| `registerCdQueue` | `game/core/cd_queue.cpp` | S1's cooperative CD loader leaves and the completion delivery |
| `sha256` | `game/core/content_identity.h` | WAD content identity |
| `registerNative*` | `game/core/native_{leaf,vec,gte,angle,rand,util}.cpp`, `game/core/native_owners.h` | Image-scoped native leaves, one installer per decomp source file |
| `guest_magnitude` | `game/core/guest_magnitude.h` | The normalise tail libgte's leaves and the collision shade share |
| `extractPanelLogo`, `TitleLogoFacts` | `game/core/title_logo.h`, `game/core/title_logo_facts.h` | A title's wordmark read out of the GPU device's VRAM for its picker panel |
| `gate_debug` | `game/core/gate_debug.h` | The `gates` and `gate-teleport` control commands |
| guest facts | `game/core/guest_globals.h`, `guest_gp.h`, `vblank_irq.h`, `memcard_*.h`, `spu_*.h`, `text_sprites.h`, `actor_mesh_scratch.h` | Guest globals, the pending-VBlank test, memory-card plans, SPU init and upload, glyph vocabulary, mesh scratch layout |

### Shared render — `game/render/`

| owner | path | owns |
|---|---|---|
| `guest_moby` | `game/render/field/guest_moby_*` | S2/S3's moby visibility walk run natively, plane tests widened by the latched presentation plan |
| `guest_render_globals::Globals` | `game/render/field/guest_render_globals.h` | The camera, ordering-table and packet-cursor words each image publishes |
| `guest_moby_frustum`, `presentedSlope`, `presentedBounds` | `game/render/field/guest_moby_frustum.h`, `game/render/field/guest_view_slope.h` | The horizontal frustum slope a sphere cull tests against and the outcode window a drawer clips to: retail's at 4:3, the latched plan's at 16:9 |
| `TerrainImage`, `Drawer`, `draw` | `game/render/terrain/guest_terrain_drawer.h` | One image's terrain drawer (facts, globals, owned windows, camera builder, passes in retail order) and the entry and exit every image shares |
| `EngineTerrainImage` | `game/render/terrain/engine_terrain_image.h` | S2/S3's drawer as a `TerrainImage`: the visibility call, then the seven passes |
| `guest_terrain` passes | `game/render/terrain/guest_terrain_*` | The S2/S3 passes, one file per pass, and the pieces S1 shares with them: the sector projector (`mesh`), sphere cull (`sphere`), sector animation (`animation`), the outcode reject and NCLIP winding tests (`cull`), far vertex projection (`far_vertices`), split grids, cracks and piece UVs (`grid`, `split`, `polygon`), fog (`fog`), re-split (`resplit`) |
| `WorkingMemory`, `GuestMemory`, `HostMemory` | `game/render/field/guest_working_memory.h` | Where a field's passes read and write: the guest's RAM for a real field, host bytes for an in-between; `packDraw`/`unpackDraw` save a draw and the words it read |
| `PacketChains`, `DrawMode` | `game/render/field/guest_packet_chain.h` | An in-between's packets walked along their links, and the draw mode its E1 packets set |
| `guest_word` | `game/render/field/guest_word.h` | R3000 word arithmetic the native ports share: signed view, `sra`, `div` quotient with the CPU's divide-by-zero results |
| `FieldWindows` | `game/render/terrain/guest_terrain_drawer.h` | The ranges a field owns: scratch, table, packet arena |
| `Rebuild`, `cameraBetween` | `game/render/terrain/terrain_rebuild.h` | The drawer run over a field's saved words at a camera between two of the guest's |
| `TerrainStateProducer` | `game/render/terrain/terrain_state_producer.h` | The terrain `StateProducer` for any `TerrainImage`: `drawField` saves the field, `render` emits the rebuilt packets into their bins |
| `depth_bins` | `game/render/terrain/guest_depth_bins.h` | The flatten's bin layout; `publishedBins` reads it, `assignBins` gives each linked packet its ordering-table slot |
| `EngineRenderFacts`, `registerEngineRenderOverrides` | `game/render/engine_render_overrides.h` | S2/S3's shared render overrides (terrain producer, moby walk, flatten) bound to one image's facts |
| `guest_camera` | `game/render/terrain/guest_camera_builder.h` | The camera builders' shared tables and packing (`Builder`), S2/S3's composition (`EngineBuilder`), re-run for an in-between |
| `ui_anchor`, `hud_draw_context` | `game/render/hud/` | The one horizontal anchoring rule for screen-space UI over the latched plan, and the widget-in-progress seam |

### Spyro 1 — `titles/spyro1/core/` (`spyro1`, leaves in `spyro1::native`)

| owner | path | owns |
|---|---|---|
| `spyro1::Runtime` | `titles/spyro1/core/runtime/runtime.h` | Crt0 image, stock library bindings (libgte's projection leaves included), native leaves, pad delivery, Record capabilities, cut, title command, input phase |
| `FieldScheduler` | `titles/spyro1/core/runtime/field_scheduler.h` | S1's `FieldObserver`: the boot window, the skip map, `presentFrame` |
| `spyro1::FrameDriver` | `titles/spyro1/core/frame/frame_driver.h` | One step: boot sequence, the retail update, `FrameDraw`, the cut sample, one present |
| `FrameDraw` | `titles/spyro1/core/frame/frame_draw.h` | GamestateDraw `0x8001ED5C` to its return; each `VSync(0)` of its tail delivered as a field |
| `BootSequence`, `TransitionSkip`, `PressLatch` | `titles/spyro1/core/frame/boot_sequence.h`, `titles/spyro1/core/frame/transition_skip.h`, `titles/spyro1/core/frame/press_latch.h` | The native boot and its logo holds; a presentation-only transition cancelled through its own terminal transition |
| `OrderingTable` | `titles/spyro1/core/frame/ordering_table.h` | LinkOTPrimitives `0x80016784`: the depth bins (table 0) and overlay pair (table 1), each linked packet's slot |
| `StageUpdateObserver`, `HandoffStoreObserver` | `titles/spyro1/core/frame/stage_update_observer.h`, `titles/spyro1/core/execution/handoff_store_observer.h` | Opt-in read-only samples after the update and at the New Game handoff |
| `ScreenOverlays`, `FullscreenTint`, `CinematicLetterbox` | `titles/spyro1/core/hud/screen_overlays.h` | `0x800190D4` and `0x80018F30` as state producers |
| `spyro1::terrain::TerrainImage`, `CameraBuilder` | `titles/spyro1/render/terrain_image.h` | RenderWorldChunks `0x800258F0` as a `TerrainImage` and its producer; the camera builder `0x80033C50` |
| S1 terrain passes | `titles/spyro1/render/terrain_{sectors,far,detail,coarse,fine,split}.cpp`, `titles/spyro1/render/terrain_facts.h` | The sector loop (sectors widened; marks and animation on retail's sphere), far, detail, coarse and fine split passes; the words and tables they read |
| `spyro1::actors` list builders | `titles/spyro1/render/actor_list.h`, `titles/spyro1/render/actor_facts.h` | BuildActorDrawList `0x8001F158` and the secondary pass `0x800208FC`, planes widened with the drawn byte +0x51 on retail's plane; the work block and record layouts every actor pass shares |
| `ActorEmitter`, `LitActorEmitter`, `ActorEmitterProducer` | `titles/spyro1/render/actor_emitter*.{h,cpp}`, `titles/spyro1/render/lit_actor_emitter.h` | The moby emitters EmitActorDrawList `0x8001F798` and EmitSecondaryActorPrimitives `0x80020F34` (lit by the secondary word: flat light or reflection), x outcodes widened, and their producer per drawn record |
| `LocalBins` | `titles/spyro1/render/local_bins.h` | The local depth bins one record draws into and their merge into the ordering table, for the emitters, sprite queue and Spyro |
| `SpriteQueue`, `SpriteQueueProducer` | `titles/spyro1/render/sprite_queue{,_producer}.h` | RasterizeSpritePrimQueue `0x80022A2C` (lit and HUD mobys), widened with +0x51 on retail's plane, and its producer per queued record |
| `PairedActor`, `PairedActorProducer` | `titles/spyro1/render/paired_actor*.{h,cpp}` | RasterizePairedActor `0x80023AC4` (Spyro) and its producer |
| actor in-betweens | `titles/spyro1/render/actor_between.h` | Record words and angle bytes at t, record bytes read and mapped, the in-between workspace and its depth bins emitted, for every S1 render producer |
| `kRetailBounds` | `titles/spyro1/render/screen.h` | Retail's outcode window, for the guest state a cull writes |
| `sky::Cyclorama`, `SkyProducer` | `titles/spyro1/render/sky{,_producer}.h` | The cyclorama drawer `0x8004EBA8` (the tail of `0x80050BD0`), widened, and its producer |
| `shadow::DropShadow`, `DropShadowProducer` | `titles/spyro1/render/drop_shadow{,_producer}.h` | DrawSpyroDropShadow `0x80059A48` and its producer |
| `shadow_block` | `titles/spyro1/core/effect/shadow_block.h` | g_SpyroShadow `0x8007AA10`'s layout, which the effect update writes and the drop shadow reads |
| `glows::GlowFans`, `Sparkles` | `titles/spyro1/render/glow{s,_producers}.h` | The glow fans `0x800580F4` and sparkles `0x800584C4` of `0x80058BA8`, widened, and their producers |
| `particles::EmitList`, `ParticleProducer` | `titles/spyro1/render/particle{s,_producer}.h` | RasterizeEmitList `0x800573C8`, widened, and its producer per record |
| `hud_anchor` | `titles/spyro1/core/hud/hud_anchor.h` | Each HUD record's and HUD quad's edge class; EmitTexturedQuad `0x8001919C`'s override |
| `InputPhase` | `titles/spyro1/core/input/input_phase.h` | Which screen takes input this pad frame, the key of a recorded `.pad` frame |
| native leaves | `titles/spyro1/core/<concept>/native_*` | One module per decomp source file, each named for the guest function it replaces |

### Spyro 2 and 3 — `titles/spyro2/`, `titles/spyro3/` (`spyro2`, `spyro3`)

| owner | path | owns |
|---|---|---|
| `spyro2::Runtime`, `spyro3::Runtime` | `titles/spyro{2,3}/core/runtime.h` | Crt0 image, stock libraries (libgte's projection leaves included) and `GuestInterruptLibcd`, boot-prefix driver, module landing, packet pool (S2), cut, logo |
| boot facts | `titles/spyro{2,3}/core/boot_facts.h` | The boot prefix, update/draw pair, field-owner and cut words, with the bytes that give each address |
| logo facts | `titles/spyro{2,3}/core/logo_facts.h` | Where the wordmark would land |
| render facts | `titles/spyro{2,3}/render/render_facts.h` | Each image's `EngineRenderFacts`: the moby walk, terrain drawer and flatten |
| `hud_anchor` | `titles/spyro{2,3}/render/hud_anchor.h` | The HUD widget drawers anchored per element class |
| manifest | `titles/spyro{1,2,3}/executable.json` | Serial, hashes, labels, environment keys |

### Build, tests, tools

| owner | path | owns |
|---|---|---|
| build | `CMakeLists.txt` | `spyro_port` through `psxport_add_product`, the title catalog generation, tests, policy gate |
| tests | `tests/` | Unit tests through shipping seams; `test_game.h` and `booted_title.h` (a title admitted through the shipping path), `record_frames.h` and `terrain_record_scene.h` (Record producer scenes), `record_wide_plan.h` (the 16:9 Record plan), `tests/spyro1/record_scene.h` (a Spyro 1 field drawn, flattened and walked) |
| verify | `tools/verify.py`, `tools/format.py`, `tools/policy/` | The gate: build, tests, clang-format, clang-tidy, ownership policy |
| launch and provisioning | `run.sh`, `tools/run.py`, `tools/provision_title.py`, `tools/generate_title_catalog.py` | The zero-argument launch, title provisioning, the `external/psxport` fetch (`tools/run.py:sync_framework`) |
| agent drivers | `tools/drive.py`, `tools/boot_run.py`, `tools/title_route.py`, `tools/shot.py`, `tools/pad_replay.py`, `tools/oracle_*.py` | Headless routes, boot runs, captures, replays, oracle comparison |

## Where new work goes

- A guest function replaced natively → the owning concept directory: `titles/spyro1/core/<concept>/`
  for one image, `game/` when S2 and S3 share the body.
- A guest draw made a producer → beside its override, installed in the session's `stateProducers`
  from `registerOverrides`, with a `composeFrame` test.
- Per-frame order or the presentation fence → the title's frame driver; field delivery →
  `FieldOwner`.
- Per-Game title state → `spyro::Session`.
- A title fact (serial, address, layout) → `titles/<title>/`, never `game/`.
- A developer command → `titleCommand`, applied at a frame boundary.
- Anything every PSX title shares → psxport, not here.
