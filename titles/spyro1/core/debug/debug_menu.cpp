#include "spyro1/core/debug/debug_menu.h"

#include "spyro1/core/debug/warp.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "core/session.h"
#include "guest_call.h"

#include <lucent/log.h>

#include <limits>

namespace spyro1 {
namespace {

using psx::game::DebugCall;
using psx::game::DebugParam;
using psx::game::WarpPlace;

constexpr std::uint32_t kFirstWorld = 1u;
constexpr std::uint32_t kLastWorld = 6u;
constexpr std::uint32_t kLevelsPerWorld = 6u;
// A world coordinate is a signed word.
constexpr std::int64_t kMinWorld = std::numeric_limits<std::int32_t>::min();
constexpr std::int64_t kMaxWorld = std::numeric_limits<std::int32_t>::max();

std::uint32_t levelIndex(std::uint32_t level) {
  return (level / 10u - 1u) * kLevelsPerWorld + level % 10u;
}

// The level is loaded and playing.
spyro::Warp::Placement placementIn(std::uint32_t level, std::function<std::string(Core &)> apply) {
  return {
      .ready =
          [level](Core &core) {
            return teleport::inActiveField(core) && core.mem_r32(guest::kLevelId) == level;
          },
      .apply = std::move(apply),
  };
}

} // namespace

DebugMenu::DebugMenu()
    : spyro::DebugMenu(
          [this](Core &core) {
            return worlds(core);
          },
          &mobys_,
          kFrameCutFacts,
          {.title = guest::kGamestateTitleScreen, .pause = guest::kGamestatePauseMenu}) {
  section("Portals and Dragons");
  verb({
      .name = "gates",
      .summary = "List the level's portals and their paths",
      .run =
          [](const DebugCall &call) {
            call.reply = teleport::listGates(call.core);
          },
      .inMenu = false,
  });
  verb({
      .name = "where",
      .summary = "Spyro's position and heading",
      .run =
          [](const DebugCall &call) {
            call.reply = teleport::where(call.core);
          },
      .inMenu = false,
  });
  verb({
      .name = "spyro-at",
      .summary = "Place Spyro at a world position and heading",
      .params = {DebugParam::integer("x", kMinWorld, kMaxWorld),
                 DebugParam::integer("y", kMinWorld, kMaxWorld),
                 DebugParam::integer("z", kMinWorld, kMaxWorld),
                 DebugParam::integer("heading", 0, teleport::kHeadingTurn - 1)},
      .run =
          [](const DebugCall &call) {
            call.reply =
                teleport::toPosition(call.core,
                                     {.x = static_cast<std::int32_t>(call.values.value(0)),
                                      .y = static_cast<std::int32_t>(call.values.value(1)),
                                      .z = static_cast<std::int32_t>(call.values.value(2))},
                                     static_cast<std::uint32_t>(call.values.value(3)));
          },
      .refusal =
          [](Core &core) {
            return teleport::inActiveField(core) ? std::string()
                                                 : "gameplay is not in an active field";
          },
      .inMenu = false,
  });
  verb({
      .name = "credits",
      .summary = "Roll the credits as the credits cheat does",
      .run =
          [](const DebugCall &call) {
            psx::cpu::callGuestNow(
                call.core, "debug-credits", guest::kCheatApplyEffect, guest::kCheatCredits);
            call.reply = "ok: credits";
          },
      .refusal =
          [](Core &core) {
            return teleport::inActiveField(core) ? std::string()
                                                 : "gameplay is not in an active field";
          },
      .inMenu = false,
  });
  verb({
      .name = "dragons",
      .summary = "List the level's crystal dragons",
      .run =
          [this](const DebugCall &call) {
            call.reply = mobys_.list(call.core, guest::kMobyClassCrystalDragon);
          },
      .inMenu = false,
  });
}

std::vector<spyro::WarpWorld> DebugMenu::worlds(Core &core) const {
  std::vector<spyro::WarpWorld> worlds;
  for (std::uint32_t world = kFirstWorld; world <= kLastWorld; ++world) {
    std::string worldName = spyro::guestName(core, guest::kTitleCardNames + world * 4u);
    if (worldName.empty()) {
      return {};
    }
    spyro::WarpWorld entry{.name = worldName};
    for (std::uint32_t level = world * 10u; level < world * 10u + kLevelsPerWorld; ++level) {
      if (!Warp::levelExists(level)) {
        continue;
      }
      const std::uint32_t index = levelIndex(level);
      const bool homeworld = level % 10u == 0u;
      spyro::WarpLevel warpLevel{
          .id = level,
          .name = homeworld ? worldName : spyro::guestName(core, guest::kLevelNames + index * 4u),
      };
      if (warpLevel.name.empty()) {
        return {};
      }
      if (!homeworld) {
        const std::uint32_t hub = world * 10u;
        warpLevel.arrivals.push_back({
            .name = "portal",
            .arrive =
                [hub, level](Core &target) {
                  return spyro::session(target).warp->arm(
                      target, hub, placementIn(hub, [level](Core &placed) {
                        return teleport::toPortal(placed, level);
                      }));
                },
        });
      }
      const std::uint32_t dragons = core.mem_r8(guest::kLevelDragonTotals + index);
      for (std::uint32_t dragon = 0; dragon < dragons; ++dragon) {
        warpLevel.arrivals.push_back({
            .name = lucent::format("dragon {}", dragon + 1u),
            .arrive =
                [this, level, dragon](Core &target) {
                  return spyro::session(target).warp->arm(
                      target, level, placementIn(level, [this, dragon](Core &placed) {
                        return mobys_.teleport(placed, guest::kMobyClassCrystalDragon, dragon);
                      }));
                },
        });
      }
      entry.levels.push_back(std::move(warpLevel));
    }
    worlds.push_back(std::move(entry));
  }
  return worlds;
}

} // namespace spyro1
