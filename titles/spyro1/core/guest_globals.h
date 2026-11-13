// The retail Spyro 1 globals, one name per address (names from external/spyro-1).
#pragma once

#include "spyro1/core/guest_gp.h"

#include <cstdint>

namespace spyro1::guest {

// g_Gamestate (common.h): the enum the main loop switches on.
inline constexpr std::uint32_t kGamestate = kGp + 0x574u; // 0x800757D8

// g_Camera: the occlusion group selects which collision groups the scene admits; the rotation is
// three halfwords read by both the camera and cutscene owners.
inline constexpr std::uint32_t kCamera = 0x80076DD0u;
inline constexpr std::uint32_t kCameraPosition = kCamera + 0x28u;       // 0x80076DF8, m_Position
inline constexpr std::uint32_t kCameraOcclusionGroup = kCamera + 0x54u; // 0x80076E24
inline constexpr std::uint32_t kCameraRotationX = kCamera + 0x4Cu;      // 0x80076E1C
inline constexpr std::uint32_t kCameraRotationY = kCamera + 0x4Eu;      // 0x80076E1E
inline constexpr std::uint32_t kCameraRotationZ = kCamera + 0x50u;      // 0x80076E20

// D_80077DD8: guest callees spill s0..s7, gp, sp, fp, ra here before reading arguments.
inline constexpr std::uint32_t kRegisterSpillArea = 0x80077DD8u;

// g_Models (moby.h): the Model pointer per class, indexed by moby +0x36; Spyro's own is entry 0.
inline constexpr std::uint32_t kModels = 0x80076378u;
// g_HudMobys and D_800756FC: screen-space mobys a frame builds (text, menus) are taken 0x58 bytes
// at a time down from the arena end, which GamestateDraw sets 0x1C000 past the draw buffer's
// packets.
inline constexpr std::uint32_t kHudMobys = 0x80075710u;
inline constexpr std::uint32_t kHudMobyArenaEnd = 0x800756FCu;
// g_DB[2], the current one (g_CurDB) and each one's packet pool pointer; a frame's built mobys are
// taken down from terrain::kPacketPoolBytes past a pool.
inline constexpr std::uint32_t kDb0 = 0x80076EE0u;
inline constexpr std::uint32_t kDb1 = 0x80076F64u;
inline constexpr std::uint32_t kCurDb = 0x80075888u;
inline constexpr std::uint32_t kDbPolyBuffer = 0x70u;
// Per collision region, the head of its moby chain.
inline constexpr std::uint32_t kMobyCollisionChain = 0x80075778u;

// g_Spyro (spyro.h): m_Position at +0, m_State at +0x78, m_controlFlags at +0x1F4.
inline constexpr std::uint32_t kSpyro = 0x80078A58u;
inline constexpr std::uint32_t kPlayerState = kSpyro + 0x78u;         // 0x80078AD0
inline constexpr std::uint32_t kPlayerControlFlags = kSpyro + 0x1F4u; // 0x80078C4C

// g_Environment (environment.h): +0x0C is the occlusion group count, +0x2C the collision header.
inline constexpr std::uint32_t kEnvironment = 0x800785A8u;
inline constexpr std::uint32_t kEnvironmentOcclusionGroupCount = kEnvironment + 0x0Cu;

// g_LoadStage (main.c): the streaming/load phase the main loop and the boot sequence both watch.
inline constexpr std::uint32_t kLoadStage = 0x80075864u;

// g_LevelTransHudActive: func_8002DA74 ends the level-transition tally by clearing it.
inline constexpr std::uint32_t kLevelTransHudActive = 0x800756B0u;

// g_CdMusic.m_Flags (music.h): 0x200 while the music fades down, 0x40 once the drive is free.
inline constexpr std::uint32_t kCdMusicFlags = 0x800774B4u;
inline constexpr std::uint32_t kCdMusicFading = 0x200u;
// g_Spu (spu.h): the CD volume CDMusicUpdate (0x8002BBE0) steps by m_MusicFadeCurrent toward
// m_MusicFadeTarget; the fade ends once the step passes the target.
inline constexpr std::uint32_t kSpuCdVolumeLeft = 0x80075F18u; // i16
inline constexpr std::uint32_t kSpuMusicFadeTarget = 0x800761D8u;
inline constexpr std::uint32_t kSpuMusicFadeCurrent = 0x80076230u;

// g_Fade: the screen fade GamestateDraw and TitlescreenDraw draw while it is non-zero.
inline constexpr std::uint32_t kFade = 0x80075918u;

// g_TitlescreenState (titlescreen.h): four-byte fields in declaration order.
inline constexpr std::uint32_t kTitlescreenState = 0x80078D78u;
inline constexpr std::uint32_t kTitlescreenMode = kTitlescreenState + 0x00u;     // m_Mode
inline constexpr std::uint32_t kTitlescreenStage = kTitlescreenState + 0x04u;    // m_State
inline constexpr std::uint32_t kTitlescreenSubTick = kTitlescreenState + 0x0Cu;  // m_SubTick
inline constexpr std::uint32_t kTitlescreenSubState = kTitlescreenState + 0x10u; // m_SubState
inline constexpr std::uint32_t kTitlescreenDemoType = kTitlescreenState + 0x1Cu; // m_DemoType
inline constexpr std::uint32_t kGamestateTitleScreen = 13u;                      // GS_TitleScreen
inline constexpr std::uint32_t kGamestatePauseMenu = 2u;                         // GS_PauseMenu
inline constexpr std::uint32_t kGamestateInventoryMenu = 3u;                     // GS_InventoryMenu
inline constexpr std::uint32_t kGamestateOldDragon = 6u;                         // GS_OldDragon
inline constexpr std::uint32_t kGamestatePlaying = 0u;                           // GS_Playing
inline constexpr std::uint32_t kGamestateFlightResults = 7u;                     // GS_FlightResults
inline constexpr std::uint32_t kGamestateDragon = 8u;                            // GS_Dragon
inline constexpr std::uint32_t kGamestateFairy = 11u;                            // GS_Fairy
inline constexpr std::uint32_t kGamestateBalloonist = 12u;                       // GS_Balloonist
inline constexpr std::uint32_t kGamestateCutscene = 14u;                         // GS_Cutscene
inline constexpr std::uint32_t kGamestateCredits = 15u;                          // GS_Credits
inline constexpr std::uint32_t kTitleModeInit = 0u;                              // TSM_Init
inline constexpr std::uint32_t kTitleModeLoading = 2u;                           // TSM_Loading
inline constexpr std::uint32_t kTitleModeDemo = 3u;                              // TSM_Demo
inline constexpr std::uint32_t kTitleStageSetup = 0u;                            // TSS_Setup
inline constexpr std::uint32_t kTitleStageActive = 2u;                           // TSS_Active
inline constexpr std::uint32_t kDemoTypeCutscene = 0u;                           // TSD_Cutscene
inline constexpr std::uint32_t kDemoTypeLevel = 1u;                              // TSD_Level
// TSM_Init sub-state 2 slides the logo in from cutscene tick 1170; 3 draws it at rest and waits
// for input; 4 slides it out, then the borders in. Each slide is a 16-byte table indexed by
// m_SubTick.
inline constexpr std::uint32_t kTitleLogoIn = 2u;
inline constexpr std::uint32_t kTitleWaiting = 3u;
inline constexpr std::uint32_t kTitleLogoOut = 4u;
// TSM_Loading's m_State 5 fades out and slides the borders away.
inline constexpr std::uint32_t kTitleLoadingLeave = 5u;
inline constexpr std::int32_t kTitleLogoInTick = 1170;
inline constexpr std::uint32_t kTitlescreenAnimIn = 0x8006FA74u;  // g_TitlescreenAnimIn
inline constexpr std::uint32_t kTitlescreenAnimOut = 0x8006FA84u; // g_TitlescreenAnimOut
inline constexpr std::uint32_t kTitlescreenAnimSteps = 16u;

// g_CutsceneLayout (loaders.c), written once at 0x80014A38; m_CurrentTick is its first word.
inline constexpr std::uint32_t kCutsceneLayout = 0x80075680u;
inline constexpr std::uint32_t kCutsceneLayoutCurrentTick = 0x00u;

// Non-zero while the respawn/game-over arm fades over a frozen gameplay frame.
inline constexpr std::uint32_t kGameplayDrawFrame = 0x8007593Cu;

// g_LevelMobys (moby.h): pointer to the level's Moby array.
inline constexpr std::uint32_t kLevelMobys = 0x80075828u;
// Moby (moby.h) m_Props, its class's props; the rest of the record is render/field/moby_record.h.
inline constexpr std::uint32_t kMobyProps = 0x00u;
// MOBYCLASS_CRYSTAL_DRAGON: the statue whose touch calls func_8002C924 into GS_Dragon.
inline constexpr std::uint32_t kMobyClassCrystalDragon = 250u;
// ACTOR_TYPE_HOME_TALKER: one per homeworld, in world order from Artisans.
inline constexpr std::uint32_t kMobyClassHomeTalker = 187u;

// D_8006CBF8: the sine table indexed with a 12-bit angle; cosine reads it a quarter turn on.
inline constexpr std::uint32_t kSinTable = 0x8006CBF8u;
inline constexpr std::uint32_t kCosTable = kSinTable + 0x80u;

// D_800770C8 (specular_and_metal.h): SpecularAndMetal.
inline constexpr std::uint32_t kSpecularAndMetal = 0x800770C8u;
inline constexpr std::uint32_t kSpecularLightDirection = kSpecularAndMetal + 0x00u;
inline constexpr std::uint32_t kSpecularLightDirectionMoby = kSpecularAndMetal + 0x0Cu;
inline constexpr std::uint32_t kMetalMatrix = kSpecularAndMetal + 0x18u;
inline constexpr std::uint32_t kSpecularTime = kSpecularAndMetal + 0x2Cu;

// D_80074B84: the reciprocal-magnitude table the guest's normalisation reads after the GTE's
// leading-zero count.
inline constexpr std::uint32_t kMagnitudeTable = 0x80074B84u;

// g_LevelId and g_NextLevelId: current and requested level identifiers.
inline constexpr std::uint32_t kLevelId = 0x8007596Cu;
inline constexpr std::uint32_t kNextLevelId = 0x800758B4u;

// external/open-spyro names: g_apTitleCardNames per homeworld (1..6), g_apLevelNameStrings at
// (id / 10 - 1) * 6 + id % 10 ("HOME" for a homeworld), g_abLevelTotalDragonsTable at the same
// index.
inline constexpr std::uint32_t kTitleCardNames = 0x8006F7D0u;
inline constexpr std::uint32_t kLevelNames = 0x8006F7F0u;
inline constexpr std::uint32_t kLevelDragonTotals = 0x8006F414u;

// g_Portals (portal.h): up to six live portal pointers.
inline constexpr std::uint32_t kPortals = 0x80078640u;
inline constexpr std::uint32_t kPortalCount = 0x800758BCu;

// The main loop's clocks: g_LevelTicks per field from VSync, g_GameTick per GS_Playing update,
// g_UnprocessedFrames, g_DeltaTime, g_StateSwitch.
inline constexpr std::uint32_t kLevelTicks = 0x800758C8u;
inline constexpr std::uint32_t kGameTick = 0x8007572Cu;
inline constexpr std::uint32_t kUnprocessedFrames = 0x80075760u;
inline constexpr std::uint32_t kDeltaTime = 0x800756CCu;
// The fields the last update took (3 and 4 stretch moby animation).
inline constexpr std::uint32_t kFrameLag = 0x800756C4u;
inline constexpr std::uint32_t kStateSwitch = 0x8007579Cu;

// g_SkipLowPolyWorld: non-zero skips the far terrain pass; the actor emitter parks it in ZSF3 for
// fog.
inline constexpr std::uint32_t kSkipLowPolyWorld = 0x8007591Cu;

// g_DemoMode (gamepad.h): DEMO_MODE_NONE/PLAY/RECORD.
inline constexpr std::uint32_t kDemoMode = 0x80075714u;

// g_Pad (gamepad.h): m_Down at +0, m_Released at +4, m_Held at +8.
inline constexpr std::uint32_t kPad = 0x80077378u;

// cheat_HomeworldSelected (cheats.c): the homeworld CheatProcessLevelWarp adds the level button to.
inline constexpr std::uint32_t kCheatHomeworldSelected = 0x800758ECu;

// g_DragonCutscene (dragon.h): state machine after 0x24-byte WAD header.
inline constexpr std::uint32_t kDragonCutscene = 0x80077030u;
inline constexpr std::uint32_t kDragonCutsceneState = kDragonCutscene + 0x28u; // m_State
inline constexpr std::uint32_t kDragonCutsceneFade = kDragonCutscene + 0x44u;  // m_Fade
inline constexpr std::uint32_t kDragonCutsceneTicks = kDragonCutscene + 0x3Cu; // m_CutsceneTicks

// D_80075744: the inventory page shown; D_800757CC slides between pages and jumps by 0x380 as this
// changes.
inline constexpr std::uint32_t kInventoryPage = 0x80075744u;

// GS_PauseMenu, GS_InventoryMenu and GS_OldDragon share the menu draw 0x8001A40C.
inline bool menuDraw(std::uint32_t gamestate) {
  return gamestate == kGamestatePauseMenu || gamestate == kGamestateInventoryMenu ||
         gamestate == kGamestateOldDragon;
}

} // namespace spyro1::guest
