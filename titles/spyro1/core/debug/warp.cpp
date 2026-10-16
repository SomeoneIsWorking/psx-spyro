#include "spyro1/core/debug/warp.h"

#include "core.h"
#include "core/guest_globals.h"
#include "guest_call.h"

namespace spyro1 {
namespace {

using spyro::guest::kGamestate;
using spyro::guest::kLevelId;
using spyro::guest::kTitlescreenState;

constexpr std::uint32_t kTitleScreen = 13u;

// g_TitlescreenState's m_Mode, m_State, m_SubState and m_DemoType.
constexpr std::uint32_t kTitleMode = kTitlescreenState + 0x00u;
constexpr std::uint32_t kTitleState = kTitlescreenState + 0x04u;
constexpr std::uint32_t kTitleSubState = kTitlescreenState + 0x10u;
constexpr std::uint32_t kTitleDemoType = kTitlescreenState + 0x1Cu;
constexpr std::uint32_t kModeInit = 0u;
constexpr std::uint32_t kModeDemo = 3u;
constexpr std::uint32_t kStateSetup = 0u;
constexpr std::uint32_t kDemoTypeLevel = 1u;
constexpr std::uint32_t kSubStateWaiting = 3u;

constexpr std::uint32_t kResetGame = 0x8001277Cu;

constexpr std::uint32_t kFirstLevel = 10u;
constexpr std::uint32_t kLevelEnd = 65u;
constexpr std::uint32_t kLastLevelOfWorld = 5u;

} // namespace

bool Warp::levelExists(std::uint32_t level) const {
  return level >= kFirstLevel && level < kLevelEnd && level % 10u <= kLastLevelOfWorld;
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGamestate) == kTitleScreen && core.mem_r32(kTitleMode) == kModeInit &&
         core.mem_r32(kTitleSubState) == kSubStateWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  psx::cpu::callGuestNow(core, "warp-reset-game", kResetGame);
  core.mem_w32(kLevelId, level);
  core.mem_w32(kTitleMode, kModeDemo);
  core.mem_w32(kTitleState, kStateSetup);
  core.mem_w32(kTitleDemoType, kDemoTypeLevel);
}

} // namespace spyro1
