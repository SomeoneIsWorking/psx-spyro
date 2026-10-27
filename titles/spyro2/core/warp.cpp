#include "spyro2/core/warp.h"

#include "spyro2/core/boot_facts.h"

#include "core.h"

#include <algorithm>
#include <array>

namespace spyro2 {
namespace {

constexpr std::uint32_t kTitleScreen = 11u;

// The title image's state, its tick and the start index state 5 passes to 0x80019230.
constexpr std::uint32_t kTitleState = 0x80067E0Cu;
constexpr std::uint32_t kTitleTick = 0x80067E00u;
constexpr std::uint32_t kStartIndex = 0x80067E14u;
constexpr std::uint32_t kTitleWaiting = 2u;
// At tick 12: title teardown at 0x8007A364, then 0x80019230(start index) at 0x8007A38C.
constexpr std::uint32_t kTitleStarting = 5u;
// Index 0 keeps the level id.
constexpr std::uint32_t kKeepLevel = 0u;

constexpr std::array<std::uint32_t, 29> kLevels{10, 11, 12, 13, 21, 22, 23, 25, 26, 30,
                                                31, 32, 33, 34, 35, 41, 42, 43, 44, 45,
                                                46, 50, 51, 52, 55, 61, 62, 65, 66};

} // namespace

bool Warp::levelExists(std::uint32_t level) {
  return std::ranges::find(kLevels, level) != kLevels.end();
}

bool Warp::titleScreenIdle(Core &core) const {
  return core.mem_r32(kGameState) == kTitleScreen && core.mem_r32(kTitleState) == kTitleWaiting;
}

void Warp::startLevel(Core &core, std::uint32_t level) const {
  core.mem_w32(kLevelId, level);
  core.mem_w32(kStartIndex, kKeepLevel);
  core.mem_w32(kTitleTick, 0u);
  core.mem_w32(kTitleState, kTitleStarting);
}

} // namespace spyro2
