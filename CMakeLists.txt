cmake_minimum_required(VERSION 3.21)
project(spyro_port CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR
    "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake; resolve the psxport checkout")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ONE LOCKED PYTHON ENVIRONMENT. Every maintainer tool and test below runs on the project's own
# `uv` environment, never on whatever interpreter is first in PATH. Two tests were failing with
# `ModuleNotFoundError: No module named 'capstone'` because `find_package(Python3)` resolved a
# user-level interpreter while the dependency lives in ./.venv -- and the fix is not to install
# anything globally, it is to run the tools on the environment that declares them.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
  message(FATAL_ERROR
          "no project Python environment at ${CMAKE_SOURCE_DIR}/.venv; create it with "
          "`uv sync --frozen` rather than installing tools into a system interpreter")
endif()
set(Python3_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python3"
    CACHE FILEPATH "Project interpreter" FORCE)

find_package(ZLIB REQUIRED)
find_package(zstd CONFIG QUIET)
if(NOT TARGET zstd::libzstd_shared)
  find_library(ZSTD_LIBRARY zstd REQUIRED)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIBRARY}")
endif()
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_subdirectory(${PSXPORT_DIR}/vendor/beetle-psx/deps/libchdr
                 ${CMAKE_BINARY_DIR}/libchdr_build EXCLUDE_FROM_ALL)
include(${PSXPORT_DIR}/cmake/psxport.cmake)

set(SPYRO_CATALOG_HEADER ${CMAKE_BINARY_DIR}/generated/spyro_title_catalog.generated.h)
add_custom_command(
  OUTPUT ${SPYRO_CATALOG_HEADER}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/generate_title_catalog.py
          --output ${SPYRO_CATALOG_HEADER}
  DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/generate_title_catalog.py
    ${CMAKE_SOURCE_DIR}/tools/title_identity.py
    ${CMAKE_SOURCE_DIR}/titles/spyro1/executable.json
    ${CMAKE_SOURCE_DIR}/titles/spyro2/executable.json
    ${CMAKE_SOURCE_DIR}/titles/spyro3/executable.json
  COMMENT "Generating executable identity catalog"
  VERBATIM)
add_custom_target(spyro_title_catalog DEPENDS ${SPYRO_CATALOG_HEADER})

set(SPYRO_RUNTIME_SOURCES
  game/core/archive_transfer.cpp
  game/core/content_identity.cpp
  game/core/cd_queue.cpp
  game/core/boot_prefix_frame_driver.cpp
  game/core/field_owner.cpp
  game/core/frame_cut.cpp
  game/core/guest_execution.cpp
  game/core/native_angle.cpp
  game/core/guest_magnitude.cpp
  game/core/native_gte.cpp
  game/core/native_leaf.cpp
  game/core/native_rand.cpp
  game/core/native_util.cpp
  game/core/native_vec.cpp
  game/core/session.cpp
  game/core/gate_debug.cpp
  game/core/debug/warp.cpp
  game/core/runtime.cpp
  game/core/title_runtime_registry.cpp
  game/core/title_logo.cpp
  game/render/terrain/guest_camera_builder.cpp
  game/render/terrain/guest_depth_bins.cpp
  game/render/terrain/terrain_rebuild.cpp
  game/render/terrain/terrain_state_producer.cpp
  game/render/hud/ui_anchor.cpp
  titles/spyro1/core/frame/boot_sequence.cpp
  titles/spyro1/core/runtime/field_scheduler.cpp
  titles/spyro1/core/frame/frame_draw.cpp
  titles/spyro1/core/frame/frame_driver.cpp
  titles/spyro1/core/frame/ordering_table.cpp
  titles/spyro1/core/input/input_phase.cpp
  titles/spyro1/core/execution/handoff_store_observer.cpp
  titles/spyro1/core/frame/stage_update_observer.cpp
  titles/spyro1/core/frame/transition_skip.cpp
  titles/spyro1/core/runtime/runtime.cpp
  titles/spyro1/core/debug/warp.cpp
  titles/spyro1/core/audio/native_sound_position.cpp
  titles/spyro1/core/audio/native_audio_key_state.cpp
  titles/spyro1/core/camera/native_camera.cpp
  titles/spyro1/core/moby/native_collision_shade.cpp
  titles/spyro1/core/player/native_cutscene.cpp
  titles/spyro1/core/frame/native_draw_setup.cpp
  titles/spyro1/core/effect/native_effect_state.cpp
  titles/spyro1/core/moby/native_environment_light.cpp
  titles/spyro1/core/input/native_gamepad.cpp
  titles/spyro1/core/hud/native_glow_sparkle_pools.cpp
  titles/spyro1/core/hud/native_hud_collectables.cpp
  titles/spyro1/core/moby/native_level_globals_table.cpp
  titles/spyro1/core/moby/native_moby_allocator.cpp
  titles/spyro1/core/moby/native_moby_collision.cpp
  titles/spyro1/core/execution/native_level_initialization.cpp
  titles/spyro1/core/moby/native_moby_helpers.cpp
  titles/spyro1/core/moby/native_moby_lists.cpp
  titles/spyro1/core/moby/native_moby_transform.cpp
  titles/spyro1/core/effect/native_particle_alloc.cpp
  titles/spyro1/core/hud/native_pause_menu.cpp
  titles/spyro1/core/hud/native_pixel_fade.cpp
  titles/spyro1/core/hud/screen_overlays.cpp
  titles/spyro1/core/hud/hud_anchor.cpp
  titles/spyro1/core/player/native_player_animation.cpp
  titles/spyro1/core/player/native_player_physics.cpp
  titles/spyro1/core/execution/native_random_range.cpp
  titles/spyro1/core/moby/native_shaded_moby_queue.cpp
  titles/spyro1/core/moby/native_shared_models.cpp
  titles/spyro1/core/audio/native_spu_registers.cpp
  titles/spyro1/core/audio/native_spu_voice_pitch.cpp
  titles/spyro1/core/audio/native_spu_state.cpp
  titles/spyro1/core/audio/native_spu_callbacks.cpp
  titles/spyro1/core/audio/native_spu_voice_attributes.cpp
  titles/spyro1/core/audio/native_spu_key_status.cpp
  titles/spyro1/core/audio/native_spu_transfer_mode.cpp
  titles/spyro1/core/audio/native_spu_common_attr.cpp
  game/render/field/guest_moby_rotation.cpp
  game/render/field/guest_moby_visibility.cpp
  game/render/field/guest_view_slope.cpp
  game/render/field/guest_working_memory.cpp
  game/render/terrain/engine_terrain_image.cpp
  game/render/terrain/guest_terrain_animation.cpp
  game/render/terrain/guest_terrain_classify.cpp
  game/render/terrain/guest_terrain_coarse.cpp
  game/render/terrain/guest_terrain_cull.cpp
  game/render/terrain/guest_terrain_detail.cpp
  game/render/terrain/guest_terrain_drawer.cpp
  game/render/terrain/guest_terrain_far.cpp
  game/render/terrain/guest_terrain_far_vertices.cpp
  game/render/terrain/guest_terrain_fine.cpp
  game/render/terrain/guest_terrain_fog.cpp
  game/render/terrain/guest_terrain_grid.cpp
  game/render/terrain/guest_terrain_mesh.cpp
  game/render/terrain/guest_terrain_resplit.cpp
  game/render/terrain/guest_terrain_sphere.cpp
  game/render/terrain/guest_terrain_split.cpp
  game/render/terrain/guest_terrain_translucent.cpp
  titles/spyro1/render/actor_between.cpp
  titles/spyro1/render/actor_emitter.cpp
  titles/spyro1/render/actor_emitter_polygons.cpp
  titles/spyro1/render/actor_emitter_producer.cpp
  titles/spyro1/render/actor_emitter_vertices.cpp
  titles/spyro1/render/actor_list.cpp
  titles/spyro1/render/local_bins.cpp
  titles/spyro1/render/lit_actor_emitter.cpp
  titles/spyro1/render/paired_actor.cpp
  titles/spyro1/render/paired_actor_polygons.cpp
  titles/spyro1/render/paired_actor_producer.cpp
  titles/spyro1/render/paired_actor_vertices.cpp
  titles/spyro1/render/drop_shadow.cpp
  titles/spyro1/render/drop_shadow_producer.cpp
  titles/spyro1/render/glow_producers.cpp
  titles/spyro1/render/glows.cpp
  titles/spyro1/render/particle_producer.cpp
  titles/spyro1/render/particles.cpp
  titles/spyro1/render/sky.cpp
  titles/spyro1/render/sky_producer.cpp
  titles/spyro1/render/sprite_queue.cpp
  titles/spyro1/render/sprite_queue_producer.cpp
  titles/spyro1/render/terrain_coarse.cpp
  titles/spyro1/render/terrain_detail.cpp
  titles/spyro1/render/terrain_far.cpp
  titles/spyro1/render/terrain_fine.cpp
  titles/spyro1/render/terrain_image.cpp
  titles/spyro1/render/terrain_sectors.cpp
  titles/spyro1/render/terrain_split.cpp
  titles/spyro2/core/runtime.cpp
  titles/spyro2/core/warp.cpp
  titles/spyro2/render/hud_anchor.cpp
  titles/spyro3/core/runtime.cpp
  titles/spyro3/core/warp.cpp
  titles/spyro3/render/hud_anchor.cpp)

add_library(spyro_runtime STATIC ${SPYRO_RUNTIME_SOURCES})
add_dependencies(spyro_runtime spyro_title_catalog)
# Includes name the owning directory: "core/runtime.h" under game/, "spyro1/render/sky.h" under titles/.
target_include_directories(spyro_runtime PUBLIC
  game
  titles
  ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(spyro_runtime PUBLIC psxport OpenSSL::Crypto)

psxport_add_product(spyro_port
  CATALOG spyro::createCatalog
  HEADER core/title_runtime_registry.h
  SOURCES game/core/world_guest_execution.cpp
  LIBRARIES spyro_runtime)
add_dependencies(spyro_port gen_gpu_shaders)
set_target_properties(spyro_port PROPERTIES
  ENABLE_EXPORTS ON
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CTest)
if(BUILD_TESTING)
  # This discriminator consumes an explicitly supplied authenticated EXE locally. No game assets
  # enter hosted CTest/CI, and no extracted instructions are committed as synthetic fixtures.
  add_executable(test_digital_input tests/test_digital_input.cpp)
  target_link_libraries(test_digital_input PRIVATE spyro_runtime psxport_test_support)
  add_executable(test_archive_transfer tests/test_archive_transfer.cpp)
  target_link_libraries(test_archive_transfer PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME archive_transfer COMMAND test_archive_transfer)
  # A missing image, so the loader-fault case reaches the production disc owner with no media.
  set_tests_properties(archive_transfer PROPERTIES ENVIRONMENT "PSXPORT_DISC=${CMAKE_BINARY_DIR}/no-disc.chd")
  foreach(test_name IN ITEMS actor_mesh_scratch archive_transfer_contract memcard_event_stack spu_hardware_init spu_pio_upload gate_debug text_sprites field_scheduler transition_skip press_latch guest_moby_frustum guest_terrain_screen guest_camera_builder guest_terrain_memory terrain_packet_window ui_anchor title_logo warp)

    add_executable(test_${test_name} tests/test_${test_name}.cpp)
    target_include_directories(test_${test_name} PRIVATE ${PSXPORT_DIR}/tests)
    target_link_libraries(test_${test_name} PRIVATE spyro_runtime psxport_test_support)
    add_test(NAME ${test_name} COMMAND test_${test_name})
  endforeach()
  # A title's own tests live under tests/<title>/; the target is named <title>_<test>.
  foreach(test_path IN ITEMS spyro1/screen_overlays spyro1/terrain spyro1/actors spyro1/effects spyro2/hud_anchor spyro3/hud_anchor)
    string(REPLACE "/" "_" test_name ${test_path})
    get_filename_component(test_dir ${test_path} DIRECTORY)
    get_filename_component(test_base ${test_path} NAME)
    add_executable(test_${test_name} tests/${test_dir}/test_${test_base}.cpp)
    target_include_directories(test_${test_name} PRIVATE ${PSXPORT_DIR}/tests tests)
    target_link_libraries(test_${test_name} PRIVATE spyro_runtime psxport_test_support)
    add_test(NAME ${test_name} COMMAND test_${test_name})
  endforeach()
  add_executable(test_stage_update_observer tests/test_stage_update_observer.cpp)
  target_include_directories(test_stage_update_observer PRIVATE ${PSXPORT_DIR}/tests)
  target_link_libraries(test_stage_update_observer PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME stage_update_observer COMMAND test_stage_update_observer)
  add_executable(test_handoff_store_observer tests/test_handoff_store_observer.cpp)
  target_include_directories(test_handoff_store_observer PRIVATE ${PSXPORT_DIR}/tests)
  target_link_libraries(test_handoff_store_observer PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME handoff_store_observer COMMAND test_handoff_store_observer)
  add_executable(test_guest_execution tests/test_guest_execution.cpp)
  target_link_libraries(test_guest_execution PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME guest_execution COMMAND test_guest_execution)
  # The shared boot-prefix frame driver, over synthetic facts: the finite guest call, one delivered
  # field per guest display wait, one presented field per step, and the refusals that keep a wedged
  # or misentered call from reading as progress.
  add_executable(test_boot_prefix_frame_driver
    tests/test_boot_prefix_frame_driver.cpp)
  target_include_directories(test_boot_prefix_frame_driver PRIVATE
    ${PSXPORT_DIR}/tests)
  target_link_libraries(test_boot_prefix_frame_driver PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME boot_prefix_frame_driver COMMAND test_boot_prefix_frame_driver)
  # Each title's measured boot-prefix facts and the runtime plan that has to agree with them, so a
  # wrong guest address fails here rather than only in a run with a disc attached.
  add_executable(test_boot_prefix_facts
    tests/test_boot_prefix_facts.cpp)
  target_include_directories(test_boot_prefix_facts PRIVATE
    ${PSXPORT_DIR}/tests)
  target_link_libraries(test_boot_prefix_facts PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME boot_prefix_facts COMMAND test_boot_prefix_facts)
  # Spyro 2's Record path: render path selection, cut declaration and terrain producer keys.
  add_executable(test_spyro2_record tests/spyro2/test_record.cpp)
  target_include_directories(test_spyro2_record PRIVATE
    ${PSXPORT_DIR}/tests tests)
  target_link_libraries(test_spyro2_record PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME spyro2_record COMMAND test_spyro2_record)
  add_executable(test_spyro1_record tests/spyro1/test_record.cpp)
  target_include_directories(test_spyro1_record PRIVATE ${PSXPORT_DIR}/tests tests)
  target_link_libraries(test_spyro1_record PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME spyro1_record COMMAND test_spyro1_record)
  add_executable(test_spyro3_record tests/spyro3/test_record.cpp)
  target_include_directories(test_spyro3_record PRIVATE
    ${PSXPORT_DIR}/tests tests)
  target_link_libraries(test_spyro3_record PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME spyro3_record COMMAND test_spyro3_record)
  # Spyro 2 and 3 stream code modules through the framework's stock CdRead and call into them: the
  # landing is published as a resident image (psxport guest_code_module.*), driven end to end
  # through the shipping read, both title runtimes, the image catalog, and Lightrec.
  add_executable(test_stock_read_publication
    tests/test_stock_read_publication.cpp)
  target_include_directories(test_stock_read_publication PRIVATE
    ${PSXPORT_DIR}/tests)
  target_link_libraries(test_stock_read_publication PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME stock_read_publication COMMAND test_stock_read_publication)
  add_executable(test_catalog tests/test_catalog.cpp)
  target_link_libraries(test_catalog PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME catalog COMMAND test_catalog)
  # Each title's startProgram over a crt0 assembled from its measured boot group.
  add_executable(test_start_program tests/test_start_program.cpp)
  target_include_directories(test_start_program PRIVATE ${PSXPORT_DIR}/tests)
  target_link_libraries(test_start_program PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME start_program COMMAND test_start_program)
  # The widened culls and HUD read the session's latched plan; each title's libgte leaves.
  add_executable(test_guest_widescreen tests/test_guest_widescreen.cpp)
  target_include_directories(test_guest_widescreen PRIVATE ${PSXPORT_DIR}/tests)
  target_link_libraries(test_guest_widescreen PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME guest_widescreen COMMAND test_guest_widescreen)
  # The phase a Spyro 1 pad recording is keyed on. Gated for the two properties a recording's
  # meaning rests on: the same guest words give the same key (so presses are interchangeable within
  # a phase), and the framework's reserved "no phase" is unreachable (so a keyed file can never
  # replay as absolute-from-boot while claiming to be keyed).
  add_executable(test_spyro1_input_phase
    tests/spyro1/test_input_phase.cpp)
  target_include_directories(test_spyro1_input_phase PRIVATE tests)
  target_link_libraries(test_spyro1_input_phase PRIVATE spyro_runtime psxport_test_support)
  add_test(NAME spyro1_input_phase COMMAND test_spyro1_input_phase)
  add_test(NAME launcher_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_launcher.py)
  add_test(NAME provision_titles
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_provision_titles.py)
  add_test(NAME cpp_ownership
    COMMAND ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py --ownership-only
            --root ${CMAKE_SOURCE_DIR} --compile-commands ${CMAKE_BINARY_DIR})
  # The agent drivers read their guest addresses out of game/core/guest_globals.h. If that parser
  # ever silently stops matching the header's form, every driver would fall back to nothing at all.
  # The Spyro 2/3 boot report. Its cases are pairs: a completed run and an aborted one (the aborted run's
  # totals must read NOT MEASURED, never 0), a matching CD count and a mismatching one, and the refusals.
  add_test(NAME boot_run_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/boot_run.py --selftest)
  # The per-title play facts and the route that uses them. Cases are pairs: a title that loads and plays
  # against one that never leaves its title, never shows one, skips loading, or whose position and
  # height words do not respond to the pad. A route that cannot refuse would report a loading screen as play.
  add_test(NAME title_profile_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/title_profile.py --selftest)
  add_test(NAME title_route_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/title_route.py --selftest)
  # A .pad replay runs with NO input driver and on a private card in the state the capture was made
  # against (docs/issues/0116). The selftest covers the card-state and environment claims here; the
  # run itself needs the disc, so it is a maintainer command (tools/pad_replay.py).
  add_test(NAME pad_replay_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/pad_replay.py --selftest)
  # Two runs of one binary on one set of inputs must be one run (docs/issues/0169). The comparison logic is
  # gated here without the disc, with the cases that matter as pairs: identical runs against runs that drift,
  # and a perturbed leg that diverges against one the digest cannot see. The live check is maintainer-run:
  # `uv run --frozen python tools/determinism_check.py --title spyro2|spyro3`.
  add_test(NAME determinism_check_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/determinism_check.py --selftest)
  add_test(NAME drive_environment_card
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_drive_environment.py)
  add_test(NAME guest_globals_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/guest_globals.py --selftest)
  # The route both the interactive driver and the oracle's level checkpoint walk. A steering table
  # that is quietly wrong still reaches destinations, by detouring; only a fixture catches it.
  add_test(NAME spyro1_steering_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/spyro1_steering.py --selftest)
  # The four scene routes the reach corpus judges on (docs/issues/0147): a flight level, a boss, a
  # death and respawn, and the in-game save. Gated for the failure they most need to not have --
  # reporting a scene they never reached. So every selftest case is a REFUSAL: a homeworld with no
  # flight or boss portal, a level with no fairy, a bounded island where nothing dies, and a guest
  # that is still at GS_Playing after every one of those. A module whose gate only proved the happy
  # path would pass for a route that walks off the edge of the map and calls it a boss fight.
  add_test(NAME route_scenes_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/route_scenes.py --selftest)
  # The walker and the recorded-route follower against a fake product: a cutscene is not a wall, a
  # distance is Spyro's and not the trailing camera's, a jump keeps its direction, and a route
  # refuses in a level it was not recorded in.
  add_test(NAME repl_walk_unit
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_repl_walk.py)
  # The pad script, which is the ONLY way a driven run can press anything on a screen it did not
  # reach with a frame count: --press-while for everything up to GS_Playing, --press-after for the
  # presentation screens that only exist after it (the level-transition tally, the entrance sweep,
  # the return-home glide). Gated because the failure it has to not have is the quiet one: a
  # condition that silently never matches looks exactly like a screen the game never reached. So its
  # selftest covers a screen that IS up (one edge out of forty samples), a screen that is NOT
  # (scanned and matched nothing, reported as such with the sample count), a re-visit (still ONE edge
  # -- gamestate 0 is both the boot logo and the arrival state, and a live run that re-fired there
  # pressed Start in gameplay and opened the pause menu), the post-arrival refusals that keep the
  # second phase off those three states for the same measured reason, and every malformed spec
  # refused by name rather than left looking armed.
  add_test(NAME press_conditions_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/press_conditions.py --selftest)
  # The menu policy the two drivers share. Gated because a driver that presses the wrong button does
  # not fail -- it walks into another menu, or opens the pause menu during gameplay, and the run still
  # reaches GS_Playing and looks fine. Its selftest pins each decision and, more importantly, the
  # negatives: the destructive card prompts are never answered, the cutscene is untouched unless
  # --skip-transitions is on, a tally that has already ended is not a screen to cancel, the title
  # menu is not the flyby card even though both are gamestate 13, and the options submenu is backed
  # out with triangle because that is where a fixed Down-count ends up.
  add_test(NAME title_prompts_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/title_prompts.py --selftest)
endif()

