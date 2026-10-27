#include "core/debug/debug_menu.h"

#include "core.h"
#include "core/debug/moby_teleport.h"
#include "core/session.h"

#include <charconv>
#include <lucent/log.h>

namespace spyro {
namespace {

using psx::game::DebugCall;
using psx::game::DebugParam;

// Every level id is a homeworld (tens) and a level in it (units).
constexpr std::uint32_t kLevelIdEnd = 100u;
// SCUS moby records hold the class as a 16-bit mesh index.
constexpr std::int64_t kMaxMobyClass = 0xFFFF;
constexpr std::int64_t kMaxMobyIndex = 255;

} // namespace

DebugMenu::DebugMenu(bool (*levelExists)(std::uint32_t), const MobyTeleport *mobys, const FrameCutFacts &scene) {
  screen(
      [scene](Core &core) {
        return (std::uint64_t{core.mem_r32(scene.gameState)} << 32) | core.mem_r32(scene.levelId);
      },
      [](Core &, std::uint64_t key) {
        return lucent::format("warp {}", static_cast<std::uint32_t>(key));
      });
  for (std::uint32_t level = 0; level < kLevelIdEnd; ++level) {
    if (levelExists(level)) {
      levels_.push_back(std::to_string(level));
    }
  }
  section("Level Warp");
  verb({
      .name = "warp",
      .summary = "Start that level from the title screen",
      .params = {DebugParam::word("level",
                                  {
                                      [this](Core &) {
                                        return static_cast<int>(levels_.size());
                                      },
                                      [this](Core &, int index) {
                                        return index >= 0 &&
                                                       index < static_cast<int>(levels_.size())
                                                   ? levels_[static_cast<std::size_t>(index)]
                                                   : std::string();
                                      },
                                  })},
      .run =
          [this](const DebugCall &call) {
            const std::string &text = levels_[static_cast<std::size_t>(call.values.value(0))];
            std::uint32_t level = 0;
            std::from_chars(text.data(), text.data() + text.size(), level);
            call.reply = session(call.core).warp->arm(level);
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
