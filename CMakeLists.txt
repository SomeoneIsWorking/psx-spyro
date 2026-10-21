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
  game/core/boot_prefix_frame_driver.cpp
  game/core/boot_prefix_runtime.cpp
  game/core/field_owner.cpp
  game/core/frame_cut.cpp
  game/core/guest_magnitude.cpp
  game/core/session.cpp
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
  titles/spyro1/core/frame/skip_map.cpp
  titles/spyro1/core/frame/frame_draw.cpp
  titles/spyro1/core/frame/frame_driver.cpp
  titles/spyro1/render/ordering_table.cpp
  titles/spyro1/core/input/input_phase.cpp
  titles/spyro1/core/frame/transition_skip.cpp
  titles/spyro1/core/runtime/runtime.cpp
  titles/spyro1/core/debug/gate_debug.cpp
  titles/spyro1/core/execution/cd_queue.cpp
  titles/spyro1/core/execution/native_angle.cpp
  titles/spyro1/core/execution/native_gte.cpp
  titles/spyro1/core/execution/native_leaf.cpp
  titles/spyro1/core/execution/native_rand.cpp
  titles/spyro1/core/execution/native_util.cpp
  titles/spyro1/core/execution/native_vec.cpp
  titles/spyro1/core/debug/warp.cpp
  titles/spyro1/core/audio/native_sound_position.cpp
  titles/spyro1/core/audio/native_audio_key_state.cpp
  titles/spyro1/core/audio/spu_library.cpp
  titles/spyro1/core/camera/native_camera.cpp
  titles/spyro1/core/moby/native_collision_shade.cpp
  titles/spyro1/core/player/native_cutscene.cpp
  titles/spyro1/core/frame/native_draw_setup.cpp
  titles/spyro1/core/effect/native_effect_state.cpp
  titles/spyro1/core/moby/native_environment_light.cpp
  titles/spyro1/core/input/native_gamepad.cpp
  titles/spyro1/render/glow_pools.cpp
  titles/spyro1/core/hud/native_hud_collectables.cpp
  titles/spyro1/core/moby/native_level_globals_table.cpp
  titles/spyro1/core/moby/native_moby_allocator.cpp
  titles/spyro1/core/moby/native_moby_collision.cpp
  titles/spyro1/core/execution/native_level_initialization.cpp
  titles/spyro1/core/moby/native_moby_animation.cpp
  titles/spyro1/core/audio/native_moby_sound.cpp
  titles/spyro1/core/moby/native_moby_lists.cpp
  titles/spyro1/core/moby/native_moby_transform.cpp
  titles/spyro1/core/effect/native_particle_alloc.cpp
  titles/spyro1/core/hud/native_pause_menu.cpp
  titles/spyro1/core/hud/native_pixel_fade.cpp
  titles/spyro1/render/screen_overlays.cpp
  titles/spyro1/render/title_sprites.cpp
  titles/spyro1/render/hud_anchor.cpp
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
  game/render/field/moby_record.cpp
  game/render/field/field_redraw.cpp
  game/render/field/loading_picture.cpp
  game/render/field/effect_pools.cpp
  game/render/field/keyframe_blend.cpp
  game/render/field/guest_moby_visibility.cpp
  game/render/field/guest_view_slope.cpp
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
  titles/spyro1/render/flame.cpp
  titles/spyro1/render/flame_producer.cpp
  titles/spyro1/render/rescue_star.cpp
  titles/spyro1/render/rescue_star_producer.cpp
  titles/spyro1/render/moby_shadows.cpp
  titles/spyro1/render/moby_shadow_producer.cpp
  titles/spyro1/render/glow_producers.cpp
  titles/spyro1/render/glows.cpp
  titles/spyro1/render/particle_producer.cpp
  titles/spyro1/render/particles.cpp
  titles/spyro1/render/portal_producer.cpp
  titles/spyro1/render/portals.cpp
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
target_link_libraries(spyro_runtime PUBLIC psxport)

psxport_add_product(spyro_port
  CATALOG spyro::createCatalog
  HEADER core/title_runtime_registry.h
  LIBRARIES spyro_runtime)
add_dependencies(spyro_port gen_gpu_shaders)
set_target_properties(spyro_port PROPERTIES
  ENABLE_EXPORTS ON
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
