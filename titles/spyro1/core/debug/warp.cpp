#include "spyro1/core/debug/warp.h"

#include "core.h"
#include "core/title_logo.h"
#include "guest_call.h"
#include "spyro1/core/debug/teleport.h"
#include "spyro1/core/frame/logo_facts.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include <array>
#include <optional>

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
// open-spyro DispatchSpyroTriggerEvent (0x80056F64): a TRIGGER_PORTAL region's data[0] is its
// level.
constexpr std::uint32_t kDispatchTriggerEvent = 0x80056F64u;
constexpr std::uint32_t kTriggerRegionTable = 0x800785B8u;
constexpr std::uint32_t kTriggerRegionCount = 0x800785BCu;
constexpr std::uint8_t kTriggerPortal = 6u;
constexpr std::uint32_t kTriggerData = 4u;
constexpr std::uint32_t kTriggerOn = 2u; // TRIGGER_EV_ON

// The current level's trigger region whose portal leads to `level`.
std::optional<std::uint32_t> portalRegionInto(Core &core, std::uint32_t level) {
  const std::uint32_t table = core.mem_r32(kTriggerRegionTable);
  const std::uint32_t count = core.mem_r32(kTriggerRegionCount);
  for (std::uint32_t region = 0; table != 0u && region < count; ++region) {
    const std::uint32_t record = core.mem_r32(table + region * 4u);
    if (core.mem_r8(record) == kTriggerPortal && core.mem_r32(record + kTriggerData) == level) {
      return region;
    }
  }
  return std::nullopt;
}

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
  // From its homeworld a level is entered through its portal, as Spyro touching the region does.
  if (core.mem_r32(kLevelId) == level / 10u * 10u) {
    if (const std::optional<std::uint32_t> region = portalRegionInto(core, level)) {
      (void)teleport::toPortal(core, level);
      psx::cpu::callGuestNow(core, "warp-portal-entry", kDispatchTriggerEvent, *region, kTriggerOn);
      return;
    }
  }
  const std::uint32_t down = core.mem_r32(kPad);
  core.mem_w32(kCheatHomeworldSelected, level / 10u);
  core.mem_w32(kPad, kLevelButtons[level % 10u]);
  psx::cpu::callGuestNow(core, "warp-level-transition", kCheatProcessLevelWarp);
  core.mem_w32(kPad, down);
}

} // namespace spyro1
