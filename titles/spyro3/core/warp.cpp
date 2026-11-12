#include "spyro3/core/warp.h"

#include "spyro3/core/boot_facts.h"
#include "spyro3/core/logo_facts.h"

#include "core.h"
#include "core/debug/portal_entry.h"
#include "core/session.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace spyro3 {
namespace {

constexpr std::uint8_t kTitleStarting = 19u;

constexpr std::uint32_t kWarpToLevel = 0x800584BCu;
// WarpToLevel's type for a level; it derives the loading picture from the id.
constexpr std::uint32_t kWarpLevel = 0u;
constexpr std::uint32_t kLevelIdToIndex = 0x80066FCCu;
// LevelIdToLevelIndex's entry for an id with no level.
constexpr std::uint32_t kNoLevel = 0x27u;
constexpr std::uint32_t kLevelHomeworlds = 0x800671A0u;
constexpr std::uint32_t kLevelNames = 0x80069CF8u;
// UpdateLoadingImage takes ids from 0x3C as cutscenes.
constexpr std::uint32_t kLevelIdEnd = 0x3Cu;
// Arriving in a homeworld from one of its levels, UpdateLoadingImage places Spyro at that level's
// portal spawn; from the slot-7 boss at the boss exit 0x80067208, from slot 8 at the homeworld's
// start.
constexpr std::uint32_t kLevelsPerHomeworld = 10u;
constexpr std::uint32_t kBossExitSlot = 7u;
constexpr std::uint32_t kStartArrivalSlot = 8u;
constexpr std::uint32_t kGameplayGameState = 0u;
// The portal entry: an index enters that portal's level; -1 leaves for this level's portal in its
// homeworld, as the pause menu's Exit Level calls it at 0x80058378.
constexpr std::uint32_t kLevelExit = 0x80053F50u;
constexpr std::uint32_t kExitToHomeworld = 0xFFFFFFFFu;
// Trigger 0x8003FD58's records and the portals its type 6 enters (0x80053F50(index) at 0x80040230).
constexpr spyro::PortalFacts kPortals{.triggerTable = 0x8006D05Cu, .portalTable = 0x8006C530u};

std::uint32_t homeworldOf(Core &core, std::uint32_t level) {
  return core.mem_r8(kLevelHomeworlds + core.mem_r8(kLevelIdToIndex + level));
}

bool arrivesAtPortal(std::uint32_t level, std::uint32_t homeworld) {
  const std::uint32_t slot = level % kLevelsPerHomeworld;
  return level > homeworld && level < homeworld + kLevelsPerHomeworld && slot != kBossExitSlot &&
         slot != kStartArrivalSlot;
}

spyro::Warp::Placement portalExit(std::uint32_t level, std::uint32_t homeworld) {
  return {
      .ready =
          [level](Core &core) {
            return core.mem_r32(kGameState) == kGameplayGameState &&
                   core.mem_r32(kLevelId) == level;
          },
      .apply =
          [homeworld](Core &core) {
            psx::cpu::callGuestNow(core, "warp-level-exit", kLevelExit, kExitToHomeworld);
            return lucent::format("left for homeworld {} at the portal", homeworld);
          },
  };
}

} // namespace

std::vector<spyro::WarpWorld> Warp::worlds(Core &core) {
  std::vector<spyro::HomeworldLevel> levels;
  for (std::uint32_t level = 0; level < kLevelIdEnd; ++level) {
    const std::uint32_t index = core.mem_r8(kLevelIdToIndex + level);
    if (index == kNoLevel) {
      continue;
    }
    std::string name = spyro::guestName(core, kLevelNames + index * 4u);
    if (name.empty()) {
      return {};
    }
    const std::uint32_t homeworld = core.mem_r8(kLevelHomeworlds + index);
    spyro::WarpLevel listed{.id = level, .name = std::move(name)};
    if (arrivesAtPortal(level, homeworld)) {
      listed.arrivals.push_back({
          .name = "portal",
          .arrive =
              [level, homeworld](Core &target) {
                return spyro::session(target).warp->arm(
                    target, level, portalExit(level, homeworld));
              },
      });
    }
    levels.push_back({.homeworld = homeworld, .level = std::move(listed)});
  }
  return spyro::groupByHomeworld(std::move(levels));
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

bool Warp::levelChangeReady(Core &core) const {
  return core.mem_r32(kGameState) == kGameplayGameState;
}

void Warp::changeLevel(Core &core, std::uint32_t level) const {
  if (core.mem_r32(kLevelId) == homeworldOf(core, level)) {
    if (const std::optional<std::uint32_t> portal = spyro::portalInto(core, kPortals, level)) {
      psx::cpu::callGuestNow(core, "warp-portal-entry", kLevelExit, *portal);
      return;
    }
  }
  psx::cpu::callGuestNow(core, "warp-to-level", kWarpToLevel, kWarpLevel, level);
}

bool Warp::playing(Core &core, std::uint32_t level) const {
  return core.mem_r32(kGameState) == kGameplayGameState && core.mem_r32(kLevelId) == level;
}

} // namespace spyro3
