#include "spyro2/core/warp.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/logo_facts.h"

#include "core.h"
#include "core/session.h"
#include "guest_call.h"

#include <lucent/log.h>

#include <optional>

namespace spyro2 {
namespace {

// The title image's tick and the start index state 5 passes to 0x80019230.
constexpr std::uint32_t kTitleTick = 0x80067E00u;
constexpr std::uint32_t kStartIndex = 0x80067E14u;
// At tick 12: title teardown at 0x8007A364, then 0x80019230(start index) at 0x8007A38C.
constexpr std::uint32_t kTitleStarting = 5u;
// Index 0 of the load starter 0x80019230 keeps the level id.
constexpr std::uint32_t kKeepLevel = 0u;
// The portal entry: a portal index enters that portal's level; a negative index leaves for this
// level's portal in its homeworld, as the pause menu's Exit Level calls it at 0x80018420.
constexpr std::uint32_t kLevelExit = 0x80019928u;
constexpr std::uint32_t kExitToHomeworld = 0xFFFFFFFFu;
// The level game state 7 fades out of and loads, set by the portal entry.
constexpr std::uint32_t kExitDestination = 0x8006715Cu;
// The boss door's entry: game state 8 into the arena at the homeworld's id + 0x10.
constexpr std::uint32_t kBossEntry = 0x8001A290u;
constexpr std::uint32_t kBossArenaOffset = 0x10u;
// Byte per level id: its index into the name and homeworld tables.
constexpr std::uint32_t kLevelIndexTable = 0x80064940u;
constexpr std::uint32_t kLevelNames = 0x800649B4u;
constexpr std::uint32_t kLevelHomeworlds = 0x80064B84u;
// The loader takes ids from 0x46 as cutscenes (0x80019364).
constexpr std::uint32_t kLevelIdEnd = 0x46u;
// The pause menu (0x800176A4) takes Exit Level only where id - 6 is not a multiple of ten; a boss
// arena leaves through its own storybook load instead.
constexpr std::uint32_t kBossSlot = 6u;
constexpr std::uint32_t kLevelsPerSlotRow = 10u;
constexpr std::uint32_t kGameplayGameState = 0u;
// The level's trigger records (pointer list, then the records it points at) and its portal records:
// a type-6 trigger calls 0x80019928(index) at 0x8002AD98, and portal `index`'s byte 0x42 is where
// it leads in a homeworld.
constexpr std::uint32_t kTriggerTable = 0x800673E8u;
constexpr std::uint8_t kPortalTrigger = 6u;
constexpr std::uint32_t kPortalTable = 0x80066F00u;
constexpr std::uint32_t kPortalRecordSize = 0x44u;
constexpr std::uint32_t kPortalDestination = 0x42u;

// The homeworld the guest tables put `level` under; 0 for an id that is no level.
std::uint32_t homeworldOf(Core &core, std::uint32_t level) {
  if (level >= kLevelIdEnd) {
    return 0u;
  }
  const std::uint32_t index = core.mem_r8(kLevelIndexTable + level);
  const std::uint32_t homeworld = core.mem_r8(kLevelHomeworlds + index);
  // Unlisted ids read index 0, which belongs only to its own homeworld.
  return index == 0u && level != homeworld ? 0u : homeworld;
}

// A level the pause menu leaves through Exit Level, arriving in front of its portal.
bool leavesForPortal(Core &core, std::uint32_t level) {
  const std::uint32_t homeworld = homeworldOf(core, level);
  return homeworld != 0u && level != homeworld && level % kLevelsPerSlotRow != kBossSlot;
}

// The index of the current level's portal into `level`, as its trigger names it.
std::optional<std::uint32_t> portalInto(Core &core, std::uint32_t level) {
  const std::uint32_t table = core.mem_r32(kTriggerTable);
  const std::uint32_t portals = core.mem_r32(kPortalTable);
  if (table == 0u || portals == 0u) {
    return std::nullopt;
  }
  const std::uint32_t records = core.mem_r32(table);
  for (std::uint32_t slot = table; slot < records; slot += 4u) {
    const std::uint32_t record = core.mem_r32(slot);
    if (core.mem_r8(record) != kPortalTrigger) {
      continue;
    }
    const std::uint32_t index = core.mem_r32(record + 4u);
    if (core.mem_r8(portals + index * kPortalRecordSize + kPortalDestination) == level) {
      return index;
    }
  }
  return std::nullopt;
}

// The lowest level id under `homeworld` with a portal there.
std::optional<std::uint32_t> firstPortalLevelOf(Core &core, std::uint32_t homeworld) {
  for (std::uint32_t level = homeworld + 1u; level < kLevelIdEnd; ++level) {
    if (homeworldOf(core, level) == homeworld && leavesForPortal(core, level)) {
      return level;
    }
  }
  return std::nullopt;
}

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
    const std::uint32_t homeworld = homeworldOf(core, level);
    if (homeworld == 0u) {
      continue;
    }
    const std::uint32_t index = core.mem_r8(kLevelIndexTable + level);
    std::string name = spyro::guestName(core, kLevelNames + index * 4u);
    if (name.empty()) {
      return {};
    }
    spyro::WarpLevel listed{.id = level, .name = std::move(name)};
    if (leavesForPortal(core, level)) {
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
  const std::uint32_t homeworld = homeworldOf(core, level);
  if (level == homeworld) {
    enterHomeworld(core, level);
    return;
  }
  if (leavesForPortal(core, level)) {
    if (core.mem_r32(kLevelId) == homeworld) {
      if (const std::optional<std::uint32_t> portal = portalInto(core, level)) {
        psx::cpu::callGuestNow(core, "warp-portal-entry", kLevelExit, *portal);
        return;
      }
    }
    psx::cpu::callGuestNow(core, "warp-level-exit", kLevelExit, kExitToHomeworld);
    core.mem_w32(kExitDestination, level);
    return;
  }
  // A boss arena loaded as an exit destination stalls in the load (0x80071BD4 phase 3); the game
  // enters it only from its homeworld through the boss door's state 8.
  if (core.mem_r32(kLevelId) != homeworld) {
    enterHomeworld(core, homeworld);
    return;
  }
  if (homeworld + kBossArenaOffset != level) {
    lucent::error(
        "warp", "level {} is not homeworld {}'s boss arena; no change entered", level, homeworld);
    return;
  }
  psx::cpu::callGuestNow(core, "warp-boss-entry", kBossEntry);
}

void Warp::enterHomeworld(Core &core, std::uint32_t homeworld) const {
  const std::uint32_t current = core.mem_r32(kLevelId);
  // A homeworld places Spyro at the portal to the level he left (0x800729B4) and reads a record
  // at address 0 when it has none, so it is entered only from a level of its own.
  if (homeworldOf(core, current) == homeworld && leavesForPortal(core, current)) {
    psx::cpu::callGuestNow(core, "warp-level-exit", kLevelExit, kExitToHomeworld);
    return;
  }
  const std::optional<std::uint32_t> entry = firstPortalLevelOf(core, homeworld);
  if (!entry) {
    lucent::error(
        "warp", "homeworld {} lists no level with a portal; no change entered", homeworld);
    return;
  }
  psx::cpu::callGuestNow(core, "warp-level-exit", kLevelExit, kExitToHomeworld);
  core.mem_w32(kExitDestination, *entry);
}

bool Warp::playing(Core &core, std::uint32_t level) const {
  return core.mem_r32(kGameState) == kGameplayGameState && core.mem_r32(kLevelId) == level;
}

} // namespace spyro2
