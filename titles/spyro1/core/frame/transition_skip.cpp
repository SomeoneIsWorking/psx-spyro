#include "spyro1/core/frame/transition_skip.h"
#include "spyro1/core/frame/guest_update_budget.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "native_dispatch.h"

#include <array>
#include <cstdlib>

#include <lucent/log.h>

namespace spyro1 {
namespace {

using guest::kCdMusicFading;
using guest::kCdMusicFlags;
using guest::kCutsceneIdx;
using guest::kCutsceneIntro;
using guest::kCutsceneLayout;
using guest::kCutsceneLayoutCurrentTick;
using guest::kCutsceneLayoutDuration;
using guest::kDeltaTime;
using guest::kDemoMode;
using guest::kDemoTypeCutscene;
using guest::kDemoTypeLevel;
using guest::kDragonCutsceneCameraLength;
using guest::kDragonCutsceneState;
using guest::kDragonCutsceneTicks;
using guest::kDragonStateSpeech;
using guest::kGamestate;
using guest::kGamestateBalloonist;
using guest::kGamestateCutscene;
using guest::kGamestateDragon;
using guest::kGamestateExitLevel;
using guest::kGamestateLevelTransition;
using guest::kGamestateTitleScreen;
using guest::kLevelTransHudActive;
using guest::kLoadStage;
using guest::kPad;
using guest::kPadBufferedDown;
using guest::kPadBufferedInputCount;
using guest::kPadBufferedInputs;
using guest::kPadBufferedInputStride;
using guest::kPadCross;
using guest::kPadHeld;
using guest::kPadStart;
using guest::kSpuCdVolumeLeft;
using guest::kSpuMusicFadeCurrent;
using guest::kSpuMusicFadeTarget;
using guest::kTitleModeDemo;
using guest::kTitlescreenDemoType;
using guest::kTitlescreenMode;
using guest::kTitlescreenStage;
using guest::kTitleStageActive;

// GS_ExitLevel: func_8002C664 is the glide's whole terminal, so it is dispatched, not copied.
constexpr std::uint32_t kReturnHome = 0x8002C664u;
constexpr std::uint64_t kReturnHomeCycles = 2u << 20;

// The TSD_Level flyby's terminal, `func_8004AC24(1); LoadLevel(1);` (0x80033158/0x80033160,
// docs/issues/0129); TSD_DemoLevel and TSD_Cutscene end elsewhere and stay uncancellable.
constexpr std::uint32_t kResetSpyroForGameplay = 0x8004AC24u;
constexpr std::uint32_t kLoadLevel = 0x80015370u;
constexpr std::uint32_t kLoadStageLevelReady = 13u;
constexpr std::uint64_t kFlybyTerminalCycles = 32u << 20;

// The TSD_Cutscene card's terminal is inline in GamestateCutsceneTransition (0x80032B08): from the
// delay slot at 0x8003304C through its epilogue, in the frame its prologue builds
// (docs/issues/0198).
constexpr psx::cpu::GuestCallSite kCutsceneTransitionCall =
    psx::cpu::GuestCallSite::fromJal(0x800339ECu, 0x80032B08u);
constexpr std::uint32_t kIntroCardTerminal = 0x8003304Cu;
constexpr std::uint32_t kCutsceneTransitionFrame = 0x38u;
constexpr std::array<GuestFrameSpill, 4> kCutsceneTransitionSpills{{
    {psx::cpu::kRa, 0x34},
    {psx::cpu::kS2, 0x30},
    {psx::cpu::kS1, 0x2C},
    {psx::cpu::kS0, 0x28},
}};
constexpr std::uint32_t kLoadStageCutsceneReady = 7u;

// Balloonist ACS_CRUISE (state 5, BalloonistCutscene_Update): the camera eases along the flight
// on the timer alone, and the state leaves for ACS_DESCEND once the timer reaches 0x100 with the
// stream at LSTREAM_FINALIZE.
constexpr std::uint32_t kActorCutsceneSubstate = 0x800777E8u;
constexpr std::uint32_t kActorCutsceneTimer = 0x800777ECu;
constexpr std::uint32_t kActorCutsceneCruise = 5u;
constexpr std::uint32_t kCruiseEnd = 0x100u;

// GamestateCutsceneUpdate's intro skip: from tick 241 with Start or Cross held, short of the last
// 32 ticks, m_Duration = (tick >> 1) + 16 fades out into EndCutscenePlayback.
constexpr std::int32_t kIntroSkipFirstTick = 241;
constexpr std::int32_t kIntroSkipTailTicks = 32;
constexpr std::int32_t kIntroSkipFadeTicks = 16;

constexpr std::uint32_t kSkipButtons = kPadStart | kPadCross;

// PadVSync's edge is ~m_Held & input: a skip button still held over the terminal's PadReset
// (LoadLevelScene) would come back as a new press. `held` is m_Held's skip buttons before it.
void consumeSkipPress(Core &core, std::uint32_t held) {
  core.mem_w32(kPad, core.mem_r32(kPad) & ~kSkipButtons);
  for (std::uint32_t slot = 0; slot < kPadBufferedInputCount; ++slot) {
    const std::uint32_t down =
        kPadBufferedInputs + slot * kPadBufferedInputStride + kPadBufferedDown;
    core.mem_w32(down, core.mem_r32(down) & ~kSkipButtons);
  }
  core.mem_w32(kPadHeld, core.mem_r32(kPadHeld) | held);
}

// Leaves through the function's own epilogue back to the frame update's call site.
void runIntroCardTerminal(Core &core) {
  const std::uint32_t entrySp = core.r[psx::cpu::kSp];
  core.r[psx::cpu::kRa] = kCutsceneTransitionCall.returnPc;
  psx::cpu::enterGuestFrame(core, kCutsceneTransitionFrame, kCutsceneTransitionSpills);
  const psx::cpu::ExecutionResult result =
      psx::cpu::resumeGuestToReturn(core,
                                    kCutsceneTransitionCall,
                                    kIntroCardTerminal,
                                    psx::cpu::ExecutionBudget::fromCycles(kGuestUpdateCycles));
  if (!psx::cpu::requireGuestReturn(result, "transition-intro-card") ||
      core.r[psx::cpu::kSp] != entrySp) {
    std::abort();
  }
}

bool musicFading(const TransitionState &state) {
  return (state.musicFlags & kCdMusicFading) != 0u && state.musicFadeTarget >= 0 &&
         state.musicFadeStep < 0;
}

// LoadLevel waits past stage 2 for the music to stop; at the target volume the next
// CDMusicUpdate ends the fade itself and pauses the drive.
void finishMusicFade(Core &core, const TransitionState &state) {
  core.mem_w16(kSpuCdVolumeLeft, static_cast<std::uint16_t>(state.musicFadeTarget));
}

} // namespace

bool balloonistCruising(const TransitionState &state) {
  return state.stage == kGamestateBalloonist && state.actorCutsceneSubstate == kActorCutsceneCruise;
}

bool flybyCardUp(const TransitionState &state) {
  return state.stage == kGamestateTitleScreen && state.titleMode == kTitleModeDemo &&
         state.titleState == kTitleStageActive &&
         (state.demoType == kDemoTypeLevel || state.demoType == kDemoTypeCutscene);
}

// Before tick 241 the guest refuses the press; the port takes that range with the guest's own
// write.
bool introCutsceneRefusesPress(const TransitionState &state) {
  return state.stage == kGamestateCutscene && state.cutsceneIdx == kCutsceneIntro &&
         state.cutsceneTick < kIntroSkipFirstTick &&
         state.cutsceneTick < state.cutsceneDuration * 2 - kIntroSkipTailTicks;
}

// func_8002F3E4 m_State 4 leaves for 5 at 0x80030D54 (voice stopped, camera on the track's last
// frame) once m_CutsceneTicks reaches twice the camera length; its own Cross skip takes that
// branch.
bool dragonSpeaking(const TransitionState &state) {
  return state.stage == kGamestateDragon && state.dragonState == kDragonStateSpeech &&
         state.dragonTicks < state.dragonCameraLength * 2;
}

Cancellation classify(const TransitionState &state) {
  if (!state.skipPressed) {
    return Cancellation::None;
  }
  // An ended tally owns no screen, so the press falls through.
  if (state.stage == kGamestateLevelTransition && state.levelTransHudActive != 0u) {
    return Cancellation::LevelTransitionTally;
  }
  if (state.stage == kGamestateLevelTransition && musicFading(state)) {
    return Cancellation::LoadingMusicFade;
  }
  if (state.stage == kGamestateExitLevel) {
    return Cancellation::ReturnHomeSequence;
  }
  // GamestateCutsceneTransition runs a card's terminal only once that card's load reaches its gate.
  if (flybyCardUp(state) && state.demoType == kDemoTypeLevel &&
      state.loadStage == kLoadStageLevelReady) {
    return Cancellation::CutsceneTransitionFlyby;
  }
  if (flybyCardUp(state) && state.demoType == kDemoTypeCutscene &&
      state.loadStage == kLoadStageCutsceneReady) {
    return Cancellation::CutsceneTransitionIntroCard;
  }
  if (balloonistCruising(state) && state.loadStage == kLoadStageLevelReady &&
      state.actorCutsceneTimer < kCruiseEnd) {
    return Cancellation::BalloonistCruise;
  }
  if (introCutsceneRefusesPress(state)) {
    return Cancellation::IntroCutscene;
  }
  if (dragonSpeaking(state)) {
    return Cancellation::DragonSpeech;
  }
  return Cancellation::None;
}

void TransitionSkip::observe(Core &core) {
  TransitionState state{
      .stage = core.mem_r32(kGamestate),
      .levelTransHudActive = core.mem_r32(kLevelTransHudActive),
      .titleMode = core.mem_r32(kTitlescreenMode),
      .titleState = core.mem_r32(kTitlescreenStage),
      .demoType = core.mem_r32(kTitlescreenDemoType),
      .loadStage = core.mem_r32(kLoadStage),
      .musicFlags = core.mem_r32(kCdMusicFlags),
      .musicFadeTarget = static_cast<std::int32_t>(core.mem_r32(kSpuMusicFadeTarget)),
      .musicFadeStep = static_cast<std::int32_t>(core.mem_r32(kSpuMusicFadeCurrent)),
      .actorCutsceneSubstate = core.mem_r32(kActorCutsceneSubstate),
      .actorCutsceneTimer = core.mem_r32(kActorCutsceneTimer),
      // The press the guest's own update reads, or a selector preview's; a recorded demo's pad is not the player's.
      .skipPressed = core.mem_r32(kDemoMode) == 0u &&
                     (core.game->session.preview() || (core.mem_r32(kPad) & kSkipButtons) != 0u)};
  if (state.stage == kGamestateCutscene) {
    const std::uint32_t layout = core.mem_r32(kCutsceneLayout);
    state.cutsceneIdx = core.mem_r32(kCutsceneIdx);
    state.cutsceneTick = static_cast<std::int32_t>(
        core.mem_r32(layout + kCutsceneLayoutCurrentTick) + core.mem_r32(kDeltaTime));
    state.cutsceneDuration =
        static_cast<std::int32_t>(core.mem_r32(layout + kCutsceneLayoutDuration));
  }
  if (state.stage == kGamestateDragon) {
    state.dragonState = core.mem_r32(kDragonCutsceneState);
    state.dragonTicks = static_cast<std::int32_t>(core.mem_r32(kDragonCutsceneTicks));
    state.dragonCameraLength = static_cast<std::int32_t>(core.mem_r32(kDragonCutsceneCameraLength));
  }

  // A card's load can outlast the one-field edge by hundreds of frames.
  if (!flybyCardUp(state)) {
    flybyPress_.clear();
  } else {
    flybyPress_.observe(state.skipPressed);
    state.skipPressed = flybyPress_.held();
  }
  // The stream can still be short of its terminal stage when the press comes.
  if (balloonistCruising(state)) {
    cruisePress_.observe(state.skipPressed);
    state.skipPressed = cruisePress_.held();
  } else {
    cruisePress_.clear();
  }

  const Cancellation cancellation = classify(state);
  if (cancellation == Cancellation::None) {
    return;
  }
  const std::uint32_t held = core.mem_r32(kPadHeld) & kSkipButtons;
  cancel(core, cancellation, state);
  consumeSkipPress(core, held);
}

void TransitionSkip::cancel(Core &core, Cancellation cancellation, const TransitionState &state) {
  switch (cancellation) {
  case Cancellation::None:
    return;
  case Cancellation::LevelTransitionTally:
    core.mem_w32(kLevelTransHudActive, 0u);
    if (musicFading(state)) {
      finishMusicFade(core, state);
    }
    ++cancellations_;
    // The tally only delays the load (func_8002DF9C), so every load phase still runs.
    lucent::info("transition",
                 "level-transition tally cancelled ({}); load continues from stage {}",
                 cancellations_,
                 core.mem_r32(kLoadStage));
    return;
  case Cancellation::LoadingMusicFade:
    finishMusicFade(core, state);
    ++cancellations_;
    lucent::info("transition",
                 "loading music fade finished ({}) at load stage {}",
                 cancellations_,
                 core.mem_r32(kLoadStage));
    return;
  case Cancellation::ReturnHomeSequence:
    psx::cpu::dispatchGuestWithArgumentsToReturn(
        core,
        kReturnHome,
        {},
        psx::cpu::ExecutionBudget::fromCycles(kReturnHomeCycles),
        "transition-return-home");
    ++cancellations_;
    lucent::info("transition",
                 "return-home glide cancelled ({}); guest 0x{:08X} left stage {} load stage {}",
                 cancellations_,
                 kReturnHome,
                 core.mem_r32(kGamestate),
                 core.mem_r32(kLoadStage));
    return;
  case Cancellation::CutsceneTransitionFlyby:
    flybyPress_.clear();
    psx::cpu::dispatchGuestWithArgumentsToReturn(
        core,
        kResetSpyroForGameplay,
        std::array<std::uint32_t, 1>{1u},
        psx::cpu::ExecutionBudget::fromCycles(kFlybyTerminalCycles),
        "transition-flyby-reset-spyro");
    psx::cpu::dispatchGuestWithArgumentsToReturn(
        core,
        kLoadLevel,
        std::array<std::uint32_t, 1>{1u},
        psx::cpu::ExecutionBudget::fromCycles(kFlybyTerminalCycles),
        "transition-flyby-load-level");
    ++cancellations_;
    lucent::info("transition",
                 "level flyby cancelled ({}); guest 0x{:08X} then 0x{:08X} left stage {} load "
                 "stage {} title mode {} state {}",
                 cancellations_,
                 kResetSpyroForGameplay,
                 kLoadLevel,
                 core.mem_r32(kGamestate),
                 core.mem_r32(kLoadStage),
                 core.mem_r32(kTitlescreenMode),
                 core.mem_r32(kTitlescreenStage));
    return;
  case Cancellation::BalloonistCruise:
    cruisePress_.clear();
    core.mem_w32(kActorCutsceneTimer, kCruiseEnd);
    ++cancellations_;
    lucent::info("transition",
                 "balloonist cruise ended ({}) at timer {:#x}; its own test leaves for the descent",
                 cancellations_,
                 state.actorCutsceneTimer);
    return;
  case Cancellation::CutsceneTransitionIntroCard:
    flybyPress_.clear();
    runIntroCardTerminal(core);
    ++cancellations_;
    lucent::info("transition",
                 "intro card cancelled ({}); guest terminal 0x{:08X} left stage {} load stage {}",
                 cancellations_,
                 kIntroCardTerminal,
                 core.mem_r32(kGamestate),
                 core.mem_r32(kLoadStage));
    return;
  case Cancellation::IntroCutscene: {
    const auto duration =
        static_cast<std::uint32_t>((state.cutsceneTick >> 1) + kIntroSkipFadeTicks);
    core.mem_w32(core.mem_r32(kCutsceneLayout) + kCutsceneLayoutDuration, duration);
    ++cancellations_;
    lucent::info("transition",
                 "intro cutscene fades out ({}) from tick {}; duration {} -> {}",
                 cancellations_,
                 state.cutsceneTick,
                 state.cutsceneDuration,
                 duration);
    return;
  }
  case Cancellation::DragonSpeech:
    core.mem_w32(kDragonCutsceneTicks, static_cast<std::uint32_t>(state.dragonCameraLength * 2));
    ++cancellations_;
    lucent::info("transition",
                 "dragon speech ended ({}) at tick {} of {}; its own test leaves for state 5",
                 cancellations_,
                 state.dragonTicks,
                 state.dragonCameraLength * 2);
    return;
  }
}

} // namespace spyro1
