#include "spyro2/core/warp.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/logo_facts.h"

#include "core.h"
#include "core/session.h"
#include "guest_call.h"

namespace spyro2 {
namespace {

// The title image's tick and the start index state 5 passes to 0x80019230.
constexpr std::uint32_t kTitleTick = 0x80067E00u;
constexpr std::uint32_t kStartIndex = 0x80067E14u;
// At tick 12: title teardown at 0x8007A364, then 0x80019230(start index) at 0x8007A38C.
constexpr std::uint32_t kTitleStarting = 5u;
// The load starter: game state 5, the storybook loader; index 0 keeps the level id.
constexpr std::uint32_t kLoadStarter = 0x80019230u;
constexpr std::uint32_t kKeepLevel = 0u;
// The loader's stream step, which the starter seeds and its in-game callers clear.
constexpr std::uint32_t kStreamStep = 0x80066EECu;
// The portal entry; a negative index leaves for this level's portal in its homeworld, as the pause
// menu's Exit Level calls it at 0x80018420.
constexpr std::uint32_t kLevelExit = 0x80019928u;
constexpr std::uint32_t kExitToHomeworld = 0xFFFFFFFFu;
// Byte per level id: its index into the name and homeworld tables.
constexpr std::uint32_t kLevelIndexTable = 0x80064940u;
constexpr std::uint32_t kLevelNames = 0x800649B4u;
constexpr std::uint32_t kLevelHomeworlds = 0x80064B84u;
// The loader takes ids from 0x46 as cutscenes (0x80019364).
constexpr std::uint32_t kLevelIdEnd = 0x46u;
// The pause menu's Exit Level leaves a boss arena (id ending in 6) through its own cutscene load.
constexpr std::uint32_t kBossSlot = 6u;
constexpr std::uint32_t kGameplayGameState = 0u;

spyro::Warp::Placement portalExit(std::uint32_t level) {
  return {
      .ready =
          [level](Core &core) {
            return core.mem_r32(kGameState) == kGameplayGameState &&
                   core.mem_r32(kLevelId) == level;
          },
      .apply =
          [](Core &core) {
            psx::cpu::callGuestNow(core, "warp-level-exit", kLevelExit, kExitToHomeworld);
            return std::string("left for the homeworld portal");
          },
  };
}

} // namespace

std::vector<spyro::WarpWorld> Warp::worlds(Core &core) {
  std::vector<spyro::HomeworldLevel> levels;
  for (std::uint32_t level = 0; level < kLevelIdEnd; ++level) {
    const std::uint32_t index = core.mem_r8(kLevelIndexTable + level);
    const std::uint32_t homeworld = core.mem_r8(kLevelHomeworlds + index);
    // Unlisted ids read index 0, which belongs only to its own homeworld.
    if (homeworld == 0u || (index == 0u && level != homeworld)) {
      continue;
    }
    std::string name = spyro::guestName(core, kLevelNames + index * 4u);
    if (name.empty()) {
      return {};
    }
    spyro::WarpLevel listed{.id = level, .name = std::move(name)};
    if (level != homeworld && level % 10u != kBossSlot) {
      listed.arrivals.push_back({
          .name = "portal",
          .arrive =
              [level](Core &target) {
                return spyro::session(target).warp->arm(target, level, portalExit(level));
              },
      });
    }
    levels.push_back({.homeworld = homeworld, .level = std::move(listed)});
  }
  return spyro::groupByHomeworld(std::move(levels));
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

bool Warp::levelChangeReady(Core &core) const {
  return core.mem_r32(kGameState) == kGameplayGameState;
}

void Warp::changeLevel(Core &core, std::uint32_t level) const {
  psx::cpu::callGuestNow(core, "warp-load-starter", kLoadStarter, kKeepLevel);
  core.mem_w32(kStreamStep, 0u);
  core.mem_w32(kLevelId, level);
}

} // namespace spyro2
