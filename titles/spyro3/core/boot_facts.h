#pragma once

#include "core/boot_prefix_frame_driver.h"
#include "spyro3/render/render_facts.h"

#include <cstdint>

namespace spyro3 {

// The draw's switch operand (lw at 0x8001E6E0).
inline constexpr std::uint32_t kGameState = 0x8006E344u;
// The title screen's state.
inline constexpr std::uint32_t kTitleScreenGameState = 11u;
// The level FUN_800584BC stores at 0x80058718.
inline constexpr std::uint32_t kLevelId = 0x8006C5BCu;
// Camera controller FUN_800144B4's mode and sub-state.
inline constexpr std::uint32_t kCameraMode = 0x8006E044u;
inline constexpr std::uint32_t kCameraSubState = 0x8006E048u;
// Level load: its draw FUN_8001DC5C draws the field (0x8001E460(0x3D), as state 0's FUN_8001D274)
// at sub-states 0 and 4 and nothing at the black load steps between.
inline constexpr spyro::LevelLoadFacts kLevelLoad{
    .gameState = 3u, .subState = 0x8006FBC8u, .fieldGameState = 0u, .fieldSubStates = {0u, 4u}};
// The state whose overlay plays a cutscene's camera track (blend test at 0x80077E0C as loaded).
inline constexpr std::uint32_t kCutsceneGameState = 6u;

// Game main 0x8001200C: boot prefix 0x8002AB38 (jal at 0x8001201C), then the loop jal 0x80055400 /
// jal 0x8001E638. Bound measurements: docs/issues/0156.
inline constexpr spyro::BootPrefixFacts kBootPrefixFacts{
    .callName = "spyro3-step",
    .bootPrefix = 0x8002AB38u,
    .frameUpdate = 0x80055400u,
    .frameDraw = 0x8001E638u,
    // FUN_8003A584 clamps the delta 0x8006C648 to [2, 4].
    .deltaClampFields = 4,
    // FUN_80020168 waits at 0x80020268.
    .fadeFrameWait = 0x80020270u,
    .field =
        {
            .titleName = "Spyro 3",
            .fieldCounter = 0,
            // No vblank root slot is measured; the framework's interrupt path owns the callback.
            .rootHandlerSlot = 0,
            // lw at 0x8001E87C in the draw 0x8001E638.
            .drawWaitFlag = 0x8006C678u,
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
            .levelLoad = kLevelLoad,
        },
    .bootStepLimit = 1024,
};

} // namespace spyro3
