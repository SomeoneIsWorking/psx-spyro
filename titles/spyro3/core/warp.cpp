#include "spyro3/core/warp.h"

#include "spyro3/core/boot_facts.h"

#include "core.h"

namespace spyro3 {
namespace {

constexpr std::uint32_t kTitleScreen = 11u;

// Title image data, resident whenever the game state is the title's: the state and its step.
constexpr std::uint32_t kTitleState = 0x8007F12Cu;
constexpr std::uint32_t kTitleStep = 0x8007F12Eu;
constexpr std::uint8_t kTitleWaiting = 1u;
constexpr std::uint8_t kTitleStarting = 19u;

constexpr std::uint32_t kFirstWorld = 1u;
constexpr std::uint32_t kLastWorld = 4u;
constexpr std::uint32_t kLevelsPerWorld = 9u;
constexpr std::uint32_t kLevelsInLastWorld = 10u;

} // namespace

bool Warp::levelExists(std::uint32_t level) const {
  const std::uint32_t world = level / 10u;
  const std::uint32_t levels = world == kLastWorld ? kLevelsInLastWorld : kLevelsPerWorld;
  return world >= kFirstWorld && world <= kLastWorld && level % 10u < levels;
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGameState) == kTitleScreen && core.mem_r8(kTitleState) == kTitleWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  core.mem_w32(kLevelId, level);
  core.mem_w8(kTitleStep, 0u);
  core.mem_w8(kTitleState, kTitleStarting);
}

} // namespace spyro3
