#include "spyro2/core/warp.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/logo_facts.h"

#include "core.h"

#include <array>

namespace spyro2 {
namespace {

// The title image's tick and the start index state 5 passes to 0x80019230.
constexpr std::uint32_t kTitleTick = 0x80067E00u;
constexpr std::uint32_t kStartIndex = 0x80067E14u;
// At tick 12: title teardown at 0x8007A364, then 0x80019230(start index) at 0x8007A38C.
constexpr std::uint32_t kTitleStarting = 5u;
// Index 0 keeps the level id.
constexpr std::uint32_t kKeepLevel = 0u;
// Byte per level id: its index into the name table.
constexpr std::uint32_t kLevelIndexTable = 0x80064940u;
constexpr std::uint32_t kLevelNames = 0x800649B4u;
// Each homeworld's levels follow its id up to the next homeworld's.
constexpr std::uint32_t kFirstHomeworld = 10u;
constexpr std::uint32_t kHomeworldSpan = 20u;

constexpr std::array<std::uint32_t, 29> kLevels{10, 11, 12, 13, 21, 22, 23, 25, 26, 30,
                                                31, 32, 33, 34, 35, 41, 42, 43, 44, 45,
                                                46, 50, 51, 52, 55, 61, 62, 65, 66};

} // namespace

std::vector<spyro::WarpWorld> Warp::worlds(Core &core) {
  std::vector<spyro::WarpWorld> worlds;
  for (const std::uint32_t level : kLevels) {
    std::string name =
        spyro::guestName(core, kLevelNames + core.mem_r8(kLevelIndexTable + level) * 4u);
    if (name.empty()) {
      return {};
    }
    if ((level - kFirstHomeworld) % kHomeworldSpan == 0u) {
      worlds.push_back({.name = name});
    }
    worlds.back().levels.push_back({.id = level, .name = std::move(name)});
  }
  return worlds;
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGameState) == kTitleScreenGameState &&
         core.mem_r32(kTitleScreenState) == kTitleScreenWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  core.mem_w32(kLevelId, level);
  core.mem_w32(kStartIndex, kKeepLevel);
  core.mem_w32(kTitleTick, 0u);
  core.mem_w32(kTitleScreenState, kTitleStarting);
}

} // namespace spyro2
