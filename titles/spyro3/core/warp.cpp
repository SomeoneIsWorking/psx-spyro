#include "spyro3/core/warp.h"

#include "spyro3/core/boot_facts.h"
#include "spyro3/core/logo_facts.h"

#include "core.h"

namespace spyro3 {
namespace {

constexpr std::uint8_t kTitleStarting = 19u;

constexpr std::uint32_t kFirstWorld = 1u;
constexpr std::uint32_t kLastWorld = 4u;
constexpr std::uint32_t kLevelsPerWorld = 9u;
constexpr std::uint32_t kLevelsInLastWorld = 10u;
constexpr std::uint32_t kLevelNames = 0x80069CF8u;

} // namespace

std::vector<spyro::WarpWorld> Warp::worlds(Core &core) {
  std::vector<spyro::WarpWorld> worlds;
  std::uint32_t index = 0;
  for (std::uint32_t world = kFirstWorld; world <= kLastWorld; ++world) {
    const std::uint32_t levels = world == kLastWorld ? kLevelsInLastWorld : kLevelsPerWorld;
    for (std::uint32_t level = world * 10u; level < world * 10u + levels; ++level, ++index) {
      std::string name = spyro::guestName(core, kLevelNames + index * 4u);
      if (name.empty()) {
        return {};
      }
      if (level == world * 10u) {
        worlds.push_back({.name = name});
      }
      worlds.back().levels.push_back({.id = level, .name = std::move(name)});
    }
  }
  return worlds;
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGameState) == kTitleScreenGameState &&
         core.mem_r8(kTitleState) == kTitleWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  core.mem_w32(kLevelId, level);
  core.mem_w8(kTitleStep, 0u);
  core.mem_w8(kTitleState, kTitleStarting);
}

} // namespace spyro3
