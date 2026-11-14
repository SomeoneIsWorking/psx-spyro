#pragma once

#include "core/boot_prefix_frame_driver.h"
#include "spyro2/core/game_state.h"
#include "spyro2/render/render_facts.h"

#include <array>
#include <cstdint>

namespace spyro2 {

// The level FUN_80019230 stores at 0x800192E8.
inline constexpr std::uint32_t kLevelId = 0x80066F90u;
// Camera controller FUN_8001FA58's mode and sub-state.
inline constexpr std::uint32_t kCameraMode = 0x80067ED0u;
inline constexpr std::uint32_t kCameraSubState = 0x80067ED4u;
// Level load FUN_8001722C: sub-state 0 fades out, 1 loads behind black, 2 fades in, then state 0.
// Its draw FUN_80014504 draws the field (0x800155A0(0x3D), as state 0's FUN_80013A14) at sub-states
// 0 and 2.
inline constexpr spyro::LevelLoadFacts kLevelLoad{
    .gameState = 3u, .subState = kGameSubState, .fieldGameState = 0u, .fieldSubStates = {0u, 2u}};
// The state whose overlay (0x800742F8) plays a cutscene's camera track.
inline constexpr std::uint32_t kCutsceneGameState = 6u;

// A negative VSync returns this guest vblank count (lw at 0x80058F40, ++ at 0x8005ACB0);
// docs/issues/0160.
inline constexpr std::uint32_t kVSyncQueryCounter = 0x80066618u;

// The boot overlay's logo sequence 0x80077374(1) (WAD 0x23000 at 0x8006D264): the Sony card's hold,
// the Universal card's two (both from one start) and the Insomniac card's, each against the start
// field it keeps at 0x80067E00.
inline constexpr std::array<spyro::LogoHold, 4> kLogoHolds{
    {{0x8007750Cu, 0xA4u}, {0x800776A4u, 0x3Cu}, {0x800778ECu, 0xB4u}, {0x80077BA0u, 0xB4u}}};

// The loading pictures FUN_80019364 indexes by 0x800698F4 that only cover a load: its sub-state 2
// enters the level (FUN_800171BC) for 0, 3, 5, 9, 13 and 16; the others start a cutscene
// (0x80073E48), open the guidebook (18) or take one of these branches.
inline constexpr std::array<spyro::LoadingKindWhen, 3> kLevelPictureBranches{{
    // 10 and 14 enter the level when it is Gulp's Overlook / Ripto's Arena.
    {.kind = 10u, .word = kLevelId, .value = 0x2Eu},
    {.kind = 14u, .word = kLevelId, .value = 0x42u},
    // 17 enters the level while 0x80067178 is clear and goes to the title when it is set.
    {.kind = 17u, .word = 0x80067178u, .value = 0u},
}};
inline constexpr spyro::LoadingKinds kLevelPictures{
    .kind = 0x800698F4u, .mask = 0x12229u, .when = kLevelPictureBranches};

// The screens a Start/Cross press ends (spyro::CountedScreenSkip): counter 0x800698EC, fade
// 0x80066F68.
inline constexpr std::array<spyro::CountedScreen, 7> kCountedScreens{{
    // FUN_80019364: ++ and leave at 0xE; at 0xB it finishes the autosave 0x80063278[0x800698F4]
    // asks for. Its draw FUN_800151F0 lights the picture count * 16 until 8.
    {.name = "the loading picture",
     .state = 5u,
     .subState = 0u,
     .word = 0x800698ECu,
     .step = 1,
     .limit = 0xD,
     .minimumKept = 3u,
     .zeroIsBefore = true,
     .mustReach = 0xB,
     .mustReachFlags = 0x80063278u,
     .mustReachKind = 0x800698F4u,
     .loading = kLevelPictures},
    // FUN_8001722C sub-state 1, behind black: ++ once the load is issued and leave past 5. A
    // respawn sets 0x800698F8 and holds to 0x4F instead.
    {.name = "the level load hold",
     .state = 3u,
     .subState = 1u,
     .word = 0x800698ECu,
     .step = 1,
     .limit = 5,
     .minimumKept = 1u,
     .zeroIsBefore = true,
     .exceptWhile = {0x800698F8u}},
    // Sub-state 2: the field fades in, -0x10 until below 1.
    {.name = "the level fade-in",
     .state = 3u,
     .subState = 2u,
     .word = 0x80066F68u,
     .step = -0x10,
     .limit = 1},
    // FUN_80019C40 sub-state 0 after Exit Level (0x80019928(-1) sets 0x800698F8): +2 per update,
    // the fade (count + 1) * 8, then the load once the count is 0x20 and FUN_8001D658 has run.
    {.name = "the level-exit fade",
     .state = 7u,
     .subState = 0u,
     .word = 0x800698ECu,
     .step = 2,
     .limit = 0x1E,
     .onlyWhile = {0x800698F8u}},
    // Sub-state 0 after a portal (0x80019928(index)): +1 per update to 0x20, the camera eased from
    // 0x8006B2D4 to 0x8006B2E0 by count / 32 and Spyro moved by 0x8006B2C4; count 0 starts its
    // sound.
    {.name = "the portal flight",
     .state = 7u,
     .subState = 0u,
     .word = 0x800698ECu,
     .step = 1,
     .limit = 0x1F,
     .zeroIsBefore = true,
     .exceptWhile = {0x800698F8u}},
    // FUN_80071BD4 sub-state 2, the portal sky: ++ to 0x8C, the camera by count / 0x8C, the fade
    // in for 8 and out from 0x85; leaving at 0x8C finishes the load it streams.
    {.name = "the portal sky",
     .state = 7u,
     .subState = 2u,
     .word = 0x800698ECu,
     .step = 1,
     .limit = 0x8B,
     .minimumKept = 8u,
     .zeroIsBefore = true,
     .offeredFrom = 8},
    // Sub-state 3, the sky under the level's name: ++ to 0x78, fade in for 8 and out from 0x70;
    // sub-state 4 finishes the load. 0x8006B2F8 is set when Triangle ends it early on a level
    // already visited.
    {.name = "the level-name sky",
     .state = 7u,
     .subState = 3u,
     .word = 0x800698ECu,
     .step = 1,
     .limit = 0x77,
     .minimumKept = 9u,
     .zeroIsBefore = true,
     .exceptWhile = {0x8006B2F8u},
     .offeredFrom = 8},
}};

// FUN_8004D810, the fade-out before a load (state 5's draw FUN_800151F0, the ending's update
// FUN_800159C8): 16 passes in one call, each darkening the drawn buffer and waiting at 0x8004D910;
// `slti $s0, 0x10` at 0x8004D948.
inline constexpr std::array<spyro::CountedLoop, 1> kCountedLoops{{
    // FUN_800151F0 calls it at sub-state 0 count 0 for the screen before and at sub-state 2 for the
    // picture.
    {.name = "the fade-out loop",
     .waitReturn = 0x8004D918u,
     .reg = 16,
     .step = 1,
     .limit = 0xF,
     .loading = kLevelPictures,
     .state = 5u,
     .subState = 2u},
}};

// Game main 0x80011ADC: boot prefix 0x80011E9C (jal at 0x80011AE4), then the loop jal 0x8001B140 /
// jal 0x800156FC. Bound measurements: docs/issues/0156.
inline constexpr spyro::BootPrefixFacts kBootPrefixFacts{
    .callName = "spyro2-step",
    .bootPrefix = 0x80011E9Cu,
    .frameUpdate = 0x8001B140u,
    .frameDraw = 0x800156FCu,
    // FUN_8001271C clamps the delta 0x80067018 to [2, 4].
    .deltaClampFields = 4,
    .fadeFrameWait = kCountedLoops[0].waitReturn,
    .logoHolds = {.holds = kLogoHolds,
                  .start = {.word = 0x80067E00u},
                  .vsyncCounter = kVSyncQueryCounter},
    .countedScreens = {.gameState = kGameState,
                       .subState = kGameSubState,
                       .screens = kCountedScreens,
                       .loops = kCountedLoops},
    .field =
        {
            .titleName = "Spyro 2",
            .fieldCounter = 0,
            // The libetc vblank callback is the BIOS HookEntryInt continuation (0x80054A78),
            // resumed by the framework's interrupt path.
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

} // namespace spyro2
