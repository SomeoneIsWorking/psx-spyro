#include "spyro1/core/frame/transition_skip.h"
#include "spyro1/core/frame/guest_update_budget.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "core/field_owner.h"
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
using guest::kDemoTypeCutscene;
using guest::kDemoTypeLevel;
using guest::kGamestate;
using guest::kGamestateBalloonist;
using guest::kGamestateTitleScreen;
using guest::kLevelTransHudActive;
using guest::kLoadStage;
using guest::kSpuCdVolumeLeft;
using guest::kSpuMusicFadeCurrent;
using guest::kSpuMusicFadeTarget;
using guest::kTitleModeDemo;
using guest::kTitlescreenDemoType;
using guest::kTitlescreenMode;
using guest::kTitlescreenStage;
using guest::kTitleStageActive;

// GS_LevelTransition, whose update is func_8002DF9C.
constexpr std::uint32_t kStageLevelTransition = 1u;

// GS_ExitLevel: func_8002C664 is the glide's whole terminal, so it is dispatched, not copied.
constexpr std::uint32_t kStageExitLevel = 10u;
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

Cancellation classify(const TransitionState &state) {
  if (!state.skipPressed) {
    return Cancellation::None;
  }
  // An ended tally owns no screen, so the press falls through.
  if (state.stage == kStageLevelTransition && state.levelTransHudActive != 0u) {
    return Cancellation::LevelTransitionTally;
  }
  if (state.stage == kStageLevelTransition && musicFading(state)) {
    return Cancellation::LoadingMusicFade;
  }
  if (state.stage == kStageExitLevel) {
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
  return Cancellation::None;
}

TransitionSkip::TransitionSkip(spyro::FieldOwner &fields) : fields_(fields) {}

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
      .skipPressed = fields_.takePresentationSkip()};

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

  switch (classify(state)) {
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
  }
}

} // namespace spyro1
