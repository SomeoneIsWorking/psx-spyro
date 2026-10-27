#include "spyro1/core/debug/debug_menu.h"

#include "spyro1/core/debug/warp.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"

namespace spyro1 {
namespace {

using psx::game::DebugCall;
using psx::game::DebugParam;

// A level holds at most 16 portals, each path at most 16 nodes.
constexpr std::int64_t kMaxPortal = 15;
constexpr std::int64_t kMaxPathNode = 15;
// Crystal dragons per level, at most.
constexpr std::int64_t kMaxDragon = 15;

std::string activeFieldRefusal(Core &core) {
  return teleport::inActiveField(core) ? std::string() : "gameplay is not in an active field";
}

} // namespace

DebugMenu::DebugMenu()
    : spyro::DebugMenu(
          &Warp::levelExists,
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
      .name = "gate-teleport",
      .summary = "Place Spyro on a portal's path",
      .params = {DebugParam::integer("portal", 0, kMaxPortal),
                 DebugParam::integer("node", 0, kMaxPathNode)},
      .run =
          [](const DebugCall &call) {
            call.reply = teleport::toGate(call.core, call.values.u32(0), call.values.u32(1));
          },
      .refusal = activeFieldRefusal,
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
  verb({
      .name = "dragon-teleport",
      .summary = "Place Spyro on a crystal dragon to rescue it",
      .params = {DebugParam::integer("dragon", 0, kMaxDragon)},
      .run =
          [this](const DebugCall &call) {
            call.reply =
                mobys_.teleport(call.core, guest::kMobyClassCrystalDragon, call.values.u32(0));
          },
      .refusal = activeFieldRefusal,
  });
}

} // namespace spyro1
