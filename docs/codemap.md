# Codemap — Spyro 1, 2 and 3

Where the code lives and what each owner owns. Feature chains are in `docs/features.md`, goals in
`docs/project-goals.md`, state in `docs/project-state.md`, binary evidence in `docs/re-frontier.md`;
the framework side is `external/psxport/docs/codemap.md`. S1 is `SCUS_942.28`, S2 `SCUS_944.25`,
S3 `SCUS_944.67`. Every source directory is a public include directory, so a header is included by
its basename.

## Owners

### Shared engine — `game/core/` (`spyro`)

| owner | path | owns |
|---|---|---|
| `SpyroCatalog`, `createCatalog` | `game/core/title_runtime_registry.h` | The product catalog: the three runtimes and their generated identities, provisioned under `scratch/assets` |
| `SpyroRuntime` | `game/core/spyro_runtime.h` | The `GameRuntime` base the three titles share: session, state producers, presentation pacing, the widescreen projection policy, panel logo, run report |
| `SpyroSession`, `session` | `game/core/spyro_session.h` | Per-Game title state: archive transfer, field owner, HUD draw context, terrain visibility and budget, state producers |
| `FieldOwner`, `FieldObserver` | `game/core/field_owner.h` | The one delivery sequence of a display field: pad, clock, VBlank root, observers, snapshot, fence, audio, host turn |
| `BootPrefixFrameDriver`, `BootPrefixFacts` | `game/core/boot_prefix_frame_driver.h` | S2/S3's `FrameDriver`: the retail boot prefix as a resumable call, then update and draw, one presented field per step; owns the title's `FrameCut` |
| `FrameCut`, `FrameCutFacts` | `game/core/frame_cut.h` | Whether a sealed record is a cut: game state, level, or (S2/S3) a camera placement inside a scene; sampled by each frame driver |
| `GuestExecution`, `WorldGuestExecution`, `GuestFrameScope` | `game/core/guest_execution.h`, `game/core/world_guest_execution.h`, `game/core/native_execution.h` | Root guest continuation across budget yields, resumed retail world work, the `$ra` of a nested guest call |
| `ArchiveTransfer`, `archive_transfer::decide` | `game/core/archive_transfer.h`, `game/core/archive_transfer_contract.h` | Bounded atomic WAD transfer with per-Core completion; the refusal decision |
| `registerCdQueue` | `game/core/cd_queue.cpp` | S1's cooperative CD loader leaves and the completion delivery |
| `sha256` | `game/core/content_identity.h` | WAD content identity |
| `registerNative*` | `game/core/native_{leaf,vec,gte,angle,rand,util}.cpp`, `game/core/spyro_game.h` | Image-scoped native leaves, one installer per decomp source file |
| `guest_magnitude` | `game/core/guest_magnitude.h` | The normalise tail libgte's leaves and the collision shade share |
| `extractPanelLogo`, `TitleLogoFacts` | `game/core/title_logo.h`, `game/core/title_logo_facts.h` | A title's wordmark read out of the GPU device's VRAM for its picker panel |
| `gate_debug` | `game/core/spyro_gate_debug.h` | The `gates` and `gate-teleport` control commands |
| guest facts | `game/core/guest_globals.h`, `guest_gp.h`, `vblank_irq.h`, `memcard_*.h`, `spu_*.h`, `text_sprites.h`, `actor_mesh_scratch.h` | Guest globals, the pending-VBlank test, memory-card plans, SPU init and upload, glyph vocabulary, mesh scratch layout |

### Shared render — `game/render/`

| owner | path | owns |
|---|---|---|
| `guest_moby` | `game/render/field/guest_moby_*` | S2/S3's moby visibility walk run natively, plane tests widened by the latched presentation plan |
| `guest_render_globals::Globals` | `game/render/field/guest_render_globals.h` | The camera, ordering-table and packet-cursor words each image publishes |
| `guest_terrain` passes | `game/render/terrain/guest_terrain_*` | The terrain drawer of this engine family, one file per pass, reading level words through `TerrainMemory` |
| `Rebuild`, `cameraBetween` | `game/render/terrain/terrain_rebuild.h` | The drawer run over a field's saved words at a camera between two of the guest's |
| `TerrainStateProducer` | `game/render/terrain/terrain_state_producer.h` | S2/S3's terrain `StateProducer`: `drawField` saves the field, `render` emits the rebuilt packets into their bins |
| `depth_bins` | `game/render/terrain/guest_depth_bins.h` | The flatten's bin layout; `publishedBins` reads it, `assignBins` gives each linked packet its ordering-table slot |
| `EngineRenderFacts`, `registerEngineRenderOverrides` | `game/render/engine_render_overrides.h` | S2/S3's shared render overrides (terrain producer, moby walk, flatten) bound to one image's facts |
| `guest_camera` | `game/render/terrain/guest_camera_builder.h` | The guest's camera builder, re-run for an in-between |
| `ui_anchor`, `hud_draw_context` | `game/render/hud/` | The one horizontal anchoring rule for screen-space UI over the latched plan, and the widget-in-progress seam |

### Spyro 1 — `titles/spyro1/core/` (`spyro1`, leaves in `spyro1::native`)

| owner | path | owns |
|---|---|---|
| `Spyro1Runtime` | `titles/spyro1/core/runtime/spyro1_runtime.h` | Crt0 image, stock library bindings (libgte's projection leaves included), native leaves, pad delivery, Record capabilities, cut, title command, input phase |
| `FieldScheduler` | `titles/spyro1/core/runtime/spyro1_field_scheduler.h` | S1's `FieldObserver`: the boot window, the skip map, `presentFrame` |
| `Spyro1FrameDriver` | `titles/spyro1/core/frame/spyro1_frame_driver.h` | One step: boot sequence, the retail update, `FrameDraw`, the cut sample, one present |
| `FrameDraw` | `titles/spyro1/core/frame/spyro1_frame_draw.h` | GamestateDraw `0x8001ED5C` to its return; each `VSync(0)` of its tail delivered as a field |
| `BootSequence`, `TransitionSkip`, `PressLatch` | `titles/spyro1/core/frame/spyro1_boot_sequence.h`, `titles/spyro1/core/frame/spyro1_transition_skip.h`, `titles/spyro1/core/frame/spyro1_press_latch.h` | The native boot and its logo holds; a presentation-only transition cancelled through its own terminal transition |
| `OrderingTable` | `titles/spyro1/core/frame/spyro1_ordering_table.h` | LinkOTPrimitives `0x80016784`: the depth bins (table 0) and overlay pair (table 1), each linked packet's slot |
| `StageUpdateObserver`, `HandoffStoreObserver` | `titles/spyro1/core/frame/stage_update_observer.h`, `titles/spyro1/core/execution/handoff_store_observer.h` | Opt-in read-only samples after the update and at the New Game handoff |
| `ScreenOverlays`, `FullscreenTint`, `CinematicLetterbox` | `titles/spyro1/core/hud/screen_overlays.h` | `0x800190D4` and `0x80018F30` as state producers |
| `InputPhase` | `titles/spyro1/core/input/spyro1_input_phase.h` | Which screen takes input this pad frame, the key of a recorded `.pad` frame |
| native leaves | `titles/spyro1/core/<concept>/native_*` | One module per decomp source file, each named for the guest function it replaces |

### Spyro 2 and 3 — `titles/spyro2/`, `titles/spyro3/` (`spyro2`, `spyro3`)

| owner | path | owns |
|---|---|---|
| `Spyro2Runtime`, `Spyro3Runtime` | `titles/spyro{2,3}/core/spyro{2,3}_runtime.h` | Crt0 image, stock libraries (libgte's projection leaves included) and `GuestInterruptLibcd`, boot-prefix driver, module landing, packet pool (S2), cut, logo |
| boot facts | `titles/spyro{2,3}/core/spyro{2,3}_boot_facts.h` | The boot prefix, update/draw pair, field-owner and cut words, with the bytes that give each address |
| logo facts | `titles/spyro{2,3}/core/spyro{2,3}_logo_facts.h` | Where the wordmark would land |
| render facts | `titles/spyro{2,3}/render/spyro{2,3}_render_facts.h` | Each image's `EngineRenderFacts`: the moby walk, terrain drawer and flatten |
| `hud_anchor` | `titles/spyro{2,3}/render/spyro{2,3}_hud_anchor.h` | The HUD widget drawers anchored per element class |
| manifest | `titles/spyro{1,2,3}/executable.json` | Serial, hashes, labels, environment keys |

### Build, tests, tools

| owner | path | owns |
|---|---|---|
| build | `CMakeLists.txt` | `spyro_port` through `psxport_add_product`, the title catalog generation, tests, policy gate |
| tests | `tests/` | Unit tests through shipping seams; `test_game.h` and `booted_title.h` (a title admitted through the shipping path), `record_frames.h` and `terrain_record_scene.h` (Record producer scenes) |
| verify | `tools/verify.py`, `tools/format.py`, `tools/policy/` | The gate: build, tests, clang-format, clang-tidy, ownership policy |
| launch and provisioning | `run.sh`, `tools/run.py`, `tools/provision_title.py`, `tools/psxport_fetch.py`, `tools/generate_title_catalog.py` | The zero-argument launch, title provisioning, the `external/psxport` link |
| agent drivers | `tools/drive.py`, `tools/boot_run.py`, `tools/title_route.py`, `tools/shot.py`, `tools/pad_replay.py`, `tools/oracle_*.py` | Headless routes, boot runs, captures, replays, oracle comparison |

## Where new work goes

- A guest function replaced natively → the owning concept directory: `titles/spyro1/core/<concept>/`
  for one image, `game/` when S2 and S3 share the body.
- A guest draw made a producer → beside its override, installed in the session's `stateProducers`
  from `registerOverrides`, with a `composeFrame` test.
- Per-frame order or the presentation fence → the title's frame driver; field delivery →
  `FieldOwner`.
- Per-Game title state → `SpyroSession`.
- A title fact (serial, address, layout) → `titles/<title>/`, never `game/`.
- A developer command → `titleCommand`, applied at a frame boundary.
- Anything every PSX title shares → psxport, not here.
