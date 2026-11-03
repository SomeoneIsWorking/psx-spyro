#include "spyro1/core/debug/warp.h"

#include "core.h"
#include "core/title_logo.h"
#include "guest_call.h"
#include "spyro1/core/frame/logo_facts.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

using guest::kDemoTypeLevel;
using guest::kInitNewGameState;
using guest::kLevelId;
using guest::kTitleModeDemo;
using guest::kTitlescreenDemoType;
using guest::kTitlescreenMode;
using guest::kTitlescreenStage;
using guest::kTitleStageSetup;

constexpr std::uint32_t kFirstLevel = 10u;
constexpr std::uint32_t kLevelEnd = 65u;
constexpr std::uint32_t kLastLevelOfWorld = 5u;

} // namespace

bool Warp::levelExists(std::uint32_t level) {
  return level >= kFirstLevel && level < kLevelEnd && level % 10u <= kLastLevelOfWorld;
}

bool Warp::titleScreenIdle(Core &core) const {
  // Spyro on the title platform with the logo up, waiting for Start or the demo timeout.
  return spyro::titleLogoGateOpen(core, kLogoFacts);
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  psx::cpu::callGuestNow(core, "warp-init-new-game", kInitNewGameState);
  core.mem_w32(kLevelId, level);
  core.mem_w32(kTitlescreenMode, kTitleModeDemo);
  core.mem_w32(kTitlescreenStage, kTitleStageSetup);
  core.mem_w32(kTitlescreenDemoType, kDemoTypeLevel);
}

} // namespace spyro1
