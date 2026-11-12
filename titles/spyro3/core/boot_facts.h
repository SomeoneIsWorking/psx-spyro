#pragma once

#include "core/boot_prefix_frame_driver.h"
#include "spyro3/core/game_state.h"
#include "spyro3/render/render_facts.h"

#include <array>
#include <cstdint>

namespace spyro3 {

// Title image data, resident whenever the game state is the title's: the state byte and its step
// byte. State 1 runs the Insomniac credit at step 0 and holds the finished logo while "press start"
// waits at step 1.
inline constexpr std::uint32_t kTitleState = 0x8007F12Cu;
inline constexpr std::uint32_t kTitleStep = 0x8007F12Eu;
inline constexpr std::uint8_t kTitleWaiting = 1u;
inline constexpr std::uint8_t kTitleStepLogoUp = 1u;
// The level FUN_800584BC stores at 0x80058718.
inline constexpr std::uint32_t kLevelId = 0x8006C5BCu;
// Camera controller FUN_800144B4's mode and sub-state.
inline constexpr std::uint32_t kCameraMode = 0x8006E044u;
inline constexpr std::uint32_t kCameraSubState = 0x8006E048u;
// Level load: its draw FUN_8001DC5C draws the field (0x8001E460(0x3D), as state 0's FUN_8001D274)
// at sub-states 0 and 4 and nothing at the black load steps between.
inline constexpr spyro::LevelLoadFacts kLevelLoad{
    .gameState = 3u, .subState = kGameSubState, .fieldGameState = 0u, .fieldSubStates = {0u, 4u}};
// The state whose overlay plays a cutscene's camera track (blend test at 0x80077E0C as loaded).
inline constexpr std::uint32_t kCutsceneGameState = 6u;

// Negative VSync reads the libetc vblank count at 0x8006B480 (docs/issues/0160).
inline constexpr std::uint32_t kVSyncQueryCounter = 0x8006B480u;

// The boot overlay's logo sequence 0x80074DEC(1) (WAD 0x23000 at 0x800742D0): the Sony card's hold,
// the Universal card's two (both from one start) and the Insomniac card's, each against the start
// field it keeps in $s5.
inline constexpr std::array<spyro::LogoHold, 4> kLogoHolds{
    {{0x80074FD8u, 0xA4u}, {0x800750B4u, 0x3Cu}, {0x800751E8u, 0xB4u}, {0x800755E0u, 0xB4u}}};

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
    .logoHolds = {.holds = kLogoHolds, .start = {.reg = 21}, .vsyncCounter = kVSyncQueryCounter},
    // FUN_80058778 leaves sub-state 0x8006FBC8 0 when 0x8006FBC4 is 0xD, then ++; its draw
    // FUN_80059038 adds the caption past count 7; sub-states 1 and 4 draw only bars.
    .loadingPicture = {.gameState = kGameState,
                       .loadingState = 5u,
                       .subState = 0x8006FBC8u,
                       .counter = 0x8006FBC4u,
                       .landingCount = 0xBu},
    .field =
        {
            .titleName = "Spyro 3",
            .fieldCounter = 0,
            // No vblank root slot is measured; the framework's interrupt path owns the callback.
            .rootHandlerSlot = 0,
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
};

} // namespace spyro3
