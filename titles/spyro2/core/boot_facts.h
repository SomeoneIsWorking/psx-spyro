#pragma once

#include "core/boot_prefix_frame_driver.h"
#include "spyro2/render/render_facts.h"

#include <cstdint>

namespace spyro2 {

// The draw's switch operand (lw in 0x800156FC).
inline constexpr std::uint32_t kGameState = 0x800681C8u;
// The level FUN_80019230 stores at 0x800192E8.
inline constexpr std::uint32_t kLevelId = 0x80066F90u;
// Camera controller FUN_8001FA58's mode and sub-state.
inline constexpr std::uint32_t kCameraMode = 0x80067ED0u;
inline constexpr std::uint32_t kCameraSubState = 0x80067ED4u;
// The state whose overlay (0x800742F8) plays a cutscene's camera track.
inline constexpr std::uint32_t kCutsceneGameState = 6u;

// Game main 0x80011ADC: boot prefix 0x80011E9C (jal at 0x80011AE4), then the loop jal 0x8001B140 /
// jal 0x800156FC. Bound measurements: docs/issues/0156.
inline constexpr spyro::BootPrefixFacts kBootPrefixFacts{
    .callName = "spyro2-step",
    .bootPrefix = 0x80011E9Cu,
    .frameUpdate = 0x8001B140u,
    .frameDraw = 0x800156FCu,
    // FUN_8001271C clamps the delta 0x80067018 to [2, 4].
    .deltaClampFields = 4,
    .field =
        {
            .titleName = "Spyro 2",
            .fieldCounter = 0,
            // The libetc vblank callback is the BIOS HookEntryInt continuation (0x80054A78),
            // resumed by the framework's interrupt path.
            .rootHandlerSlot = 0,
            // Read before the wait loop of the draw 0x800156FC.
            .drawWaitFlag = 0x80067044u,
        },
    .cut =
        {
            .gameState = kGameState,
            .levelId = kLevelId,
            .camera = spyro::CameraMachineWords{.mode = kCameraMode, .subState = kCameraSubState},
            .scriptedCamera =
                spyro::ScriptedCameraFacts{
                    .gameState = kCutsceneGameState,
                    .position = kRenderGlobals.cameraPosition,
                    .angles = kRenderGlobals.cameraAngles,
                },
        },
};

} // namespace spyro2
