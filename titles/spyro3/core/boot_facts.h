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

// The loading pictures FUN_80058778 indexes by 0x8006FBCC that only cover a load: its sub-state 3
// enters a level for 0..4; 5 on are story cards.
inline constexpr spyro::LoadingKinds kLevelPictures{.kind = 0x8006FBCCu, .mask = 0x1Fu};

// The screens a Start/Cross press ends (spyro::CountedScreenSkip): counter 0x8006FBC4, fade
// 0x8006C598.
inline constexpr std::array<spyro::CountedScreen, 6> kCountedScreens{{
    // FUN_80058778 leaves when it reads 0xD, then ++; its draw FUN_80059038 lights the picture
    // count * 16 until 8 and adds the caption past 7; sub-states 1 and 4 draw only bars.
    {.name = "the loading picture",
     .state = 5u,
     .subState = 0u,
     .word = 0x8006FBC4u,
     .step = 1,
     .limit = 0xD,
     .minimumKept = 3u,
     .zeroIsBefore = true,
     .loading = kLevelPictures},
    // FUN_80053A10 sub-state 4: the field fades in, -0x10 until below 1.
    {.name = "the level fade-in",
     .state = 3u,
     .subState = 4u,
     .word = 0x8006C598u,
     .step = -0x10,
     .limit = 1},
    // FUN_80054450 sub-state 0 after Exit Level (0x80053F50(-1) sets 0x8006FBD0) or while
    // 0x8007056C is set: +2 per update, the fade (count + 1) * 8, then the load once the count is
    // 0x20 and FUN_8002CA50 has run.
    {.name = "the level-exit fade",
     .state = 7u,
     .subState = 0u,
     .word = 0x8006FBC4u,
     .step = 2,
     .limit = 0x1E,
     .onlyWhile = {0x8006FBD0u, 0x8007056Cu}},
    // Sub-state 0 after a portal (0x80053F50(index)): +1 per update to 0x20, the camera eased from
    // 0x80072308 to 0x80072314 by count / 32 and Spyro moved by 0x800722F8.
    {.name = "the portal flight",
     .state = 7u,
     .subState = 0u,
     .word = 0x8006FBC4u,
     .step = 1,
     .limit = 0x1F,
     .zeroIsBefore = true,
     .exceptWhile = {0x8006FBD0u, 0x8007056Cu}},
    // Portal overlay 0x80076538 sub-state 3, the portal sky: ++ to 0x8C, the fade in for 8 and
    // out from 0x85 while FUN_8002CA50 streams the load.
    {.name = "the portal sky",
     .state = 7u,
     .subState = 3u,
     .word = 0x8006FBC4u,
     .step = 1,
     .limit = 0x8B,
     .minimumKept = 8u,
     .zeroIsBefore = true,
     .offeredFrom = 8},
    // Sub-state 4, the sky under the level's name: ++ to 0x78, fade in for 8 and out from 0x70;
    // leaving finishes the load and the autosave (stage 7 of FUN_8002CA50).
    {.name = "the level-name sky",
     .state = 7u,
     .subState = 4u,
     .word = 0x8006FBC4u,
     .step = 1,
     .limit = 0x77,
     .minimumKept = 9u,
     .zeroIsBefore = true,
     .offeredFrom = 8},
}};

// FUN_80020168, the loading picture's fade-out (state 5 sub-state 3): 16 passes in one call, each
// darkening the drawn buffer and waiting at 0x80020268; `slti $s0, 0x10` at 0x800202A0.
inline constexpr std::array<spyro::CountedLoop, 1> kCountedLoops{{
    // Only from FUN_80059038's sub-state 3, after the picture 0x8006FBCC indexes.
    {.name = "the fade-out loop",
     .waitReturn = 0x80020270u,
     .reg = 16,
     .step = 1,
     .limit = 0xF,
     .loading = kLevelPictures,
     .state = 5u,
     .subState = 3u},
}};

// Game main 0x8001200C: boot prefix 0x8002AB38 (jal at 0x8001201C), then the loop jal 0x80055400 /
// jal 0x8001E638. Bound measurements: docs/issues/0156.
inline constexpr spyro::BootPrefixFacts kBootPrefixFacts{
    .callName = "spyro3-step",
    .bootPrefix = 0x8002AB38u,
    .frameUpdate = 0x80055400u,
    .frameDraw = 0x8001E638u,
    // FUN_8003A584 clamps the delta 0x8006C648 to [2, 4].
    .deltaClampFields = 4,
    .fadeFrameWait = kCountedLoops[0].waitReturn,
    .logoHolds = {.holds = kLogoHolds, .start = {.reg = 21}, .vsyncCounter = kVSyncQueryCounter},
    .countedScreens = {.gameState = kGameState,
                       .subState = kGameSubState,
                       .screens = kCountedScreens,
                       .loops = kCountedLoops},
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
