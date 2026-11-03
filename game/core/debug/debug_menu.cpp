#include "core/debug/debug_menu.h"

#include "core.h"
#include "core/debug/moby_teleport.h"
#include "core/session.h"
#include "game.h"

#include <lucent/log.h>

#include <array>
#include <utility>

namespace spyro {
namespace {

using psx::game::DebugCall;
using psx::game::DebugParam;
using psx::game::WarpPlace;

// SCUS moby records hold the class as a 16-bit mesh index.
constexpr std::int64_t kMaxMobyClass = 0xFFFF;
constexpr std::int64_t kMaxMobyIndex = 255;
// Longest level name in the three executables, with room.
constexpr std::size_t kNameBytes = 40;

} // namespace

std::string guestName(Core &core, std::uint32_t pointerWord) {
  const std::uint32_t text = core.mem_r32(pointerWord);
  if (!core.isGuestStorage(text, 1u)) {
    return {};
  }
  std::array<char, kNameBytes> name{};
  core.readCString(text, name.data(), name.size());
  return {name.data()};
}

DebugMenu::DebugMenu(WarpWorlds worlds,
                     const MobyTeleport *mobys,
                     const FrameCutFacts &scene,
                     const ScreenStates &states) {
  // Route steps run in order, each once the screen the one before it reaches is shown.
  screen(
      [scene](Core &core) {
        return (std::uint64_t{core.mem_r32(scene.gameState)} << 32) | core.mem_r32(scene.levelId);
      },
      [this, states](Core &core, std::uint64_t key) {
        const auto state = static_cast<std::uint32_t>(key >> 32);
        const auto level = static_cast<std::uint32_t>(key);
        if (state == states.title && core.game->title != nullptr) {
          return lucent::format("pick {}", core.game->title->slug);
        }
        warpPlaces(core);
        const auto path = levelPaths_.find(level);
        if (path == levelPaths_.end()) {
          return std::string();
        }
        if (state == states.playing) {
          return lucent::format("warp {}", path->second);
        }
        if (states.pause && state == *states.pause) {
          return lucent::format("warp {}; tap start", path->second);
        }
        return std::string();
      });
  warp({
      .levels = {"homeworld", "level", "arrival"},
      .places =
          [this, worlds = std::move(worlds)](Core &core) {
            std::vector<WarpPlace> places;
            levelPaths_.clear();
            for (WarpWorld &world : worlds(core)) {
              WarpPlace worldPlace{.name = std::move(world.name)};
              for (WarpLevel &level : world.levels) {
                levelPaths_[level.id] =
                    lucent::format("{}/{}/0", places.size(), worldPlace.places.size());
                WarpPlace levelPlace{.name = std::move(level.name)};
                levelPlace.places.push_back({
                    .name = "start",
                    .arrive =
                        [id = level.id](Core &target) {
                          return session(target).warp->arm(target, id);
                        },
                });
                for (WarpPlace &arrival : level.arrivals) {
                  levelPlace.places.push_back(std::move(arrival));
                }
                worldPlace.places.push_back(std::move(levelPlace));
              }
              places.push_back(std::move(worldPlace));
            }
            return places;
          },
  });
  if (mobys == nullptr) {
    return;
  }
  section("Moby Teleport");
  const auto refusal = [mobys](Core &core) {
    return mobys->inActiveField(core) ? std::string() : "gameplay is not in an active field";
  };
  verb({
      .name = "mobys",
      .summary = "List the level's mobys of one class",
      .params = {DebugParam::integer("class", 0, kMaxMobyClass)},
      .run =
          [mobys](const DebugCall &call) {
            call.reply = mobys->list(call.core, call.values.u32(0));
          },
      .inMenu = false,
  });
  verb({
      .name = "moby-teleport",
      .summary = "Place Spyro on a moby of one class",
      .params = {DebugParam::integer("class", 0, kMaxMobyClass),
                 DebugParam::integer("index", 0, kMaxMobyIndex)},
      .run =
          [mobys](const DebugCall &call) {
            call.reply = mobys->teleport(call.core, call.values.u32(0), call.values.u32(1));
          },
      .refusal = refusal,
      .inMenu = false,
  });
}

} // namespace spyro
