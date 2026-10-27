#include "spyro1/core/debug/warp.h"

#include "core.h"
#include "guest_call.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

using guest::kDemoTypeLevel;
using guest::kGamestate;
using guest::kGamestateTitleScreen;
using guest::kInitNewGameState;
using guest::kLevelId;
using guest::kTitleModeDemo;
using guest::kTitleModeInit;
using guest::kTitlescreenDemoType;
using guest::kTitlescreenMode;
using guest::kTitlescreenStage;
using guest::kTitlescreenSubState;
using guest::kTitleStageSetup;

// Spyro on the title platform, waiting for Start or the demo timeout.
constexpr std::uint32_t kSubStateWaiting = 3u;

constexpr std::uint32_t kFirstLevel = 10u;
constexpr std::uint32_t kLevelEnd = 65u;
constexpr std::uint32_t kLastLevelOfWorld = 5u;

} // namespace

bool Warp::levelExists(std::uint32_t level) {
  return level >= kFirstLevel && level < kLevelEnd && level % 10u <= kLastLevelOfWorld;
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGamestate) == kGamestateTitleScreen && core.mem_r32(kTitlescreenMode) == kTitleModeInit &&
         core.mem_r32(kTitlescreenSubState) == kSubStateWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  psx::cpu::callGuestNow(core, "warp-init-new-game", kInitNewGameState);
  core.mem_w32(kLevelId, level);
  core.mem_w32(kTitlescreenMode, kTitleModeDemo);
  core.mem_w32(kTitlescreenStage, kTitleStageSetup);
  core.mem_w32(kTitlescreenDemoType, kDemoTypeLevel);
}

} // namespace spyro1
