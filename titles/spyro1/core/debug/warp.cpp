#include "spyro1/core/debug/warp.h"

#include "core.h"
#include "core/title_logo.h"
#include "guest_call.h"
#include "spyro1/core/frame/logo_facts.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include <array>

namespace spyro1 {
namespace {

using guest::kCheatHomeworldSelected;
using guest::kCheatProcessLevelWarp;
using guest::kDemoTypeLevel;
using guest::kGamestate;
using guest::kGamestatePlaying;
using guest::kInitNewGameState;
using guest::kLevelId;
using guest::kLoadStage;
using guest::kPad;
using guest::kTitleModeDemo;
using guest::kTitlescreenDemoType;
using guest::kTitlescreenMode;
using guest::kTitlescreenStage;
using guest::kTitleStageSetup;

constexpr std::uint32_t kFirstLevel = 10u;
constexpr std::uint32_t kLevelEnd = 65u;
constexpr std::uint32_t kLastLevelOfWorld = 5u;
// CheatProcessLevelWarp's level per button: circle, cross, square, triangle, right, down.
constexpr std::array<std::uint32_t, kLastLevelOfWorld + 1u> kLevelButtons{
    0x0020u, 0x0040u, 0x0080u, 0x0010u, 0x2000u, 0x4000u};

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

bool Warp::levelChangeReady(Core &core) const {
  return core.mem_r32(kGamestate) == kGamestatePlaying &&
         static_cast<std::int32_t>(core.mem_r32(kLoadStage)) < 0;
}

void Warp::changeLevel(Core &core, std::uint32_t level) const {
  const std::uint32_t down = core.mem_r32(kPad);
  core.mem_w32(kCheatHomeworldSelected, level / 10u);
  core.mem_w32(kPad, kLevelButtons[level % 10u]);
  psx::cpu::callGuestNow(core, "warp-level-transition", kCheatProcessLevelWarp);
  core.mem_w32(kPad, down);
}

} // namespace spyro1
