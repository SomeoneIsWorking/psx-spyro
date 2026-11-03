#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/warp.h"

#include <vector>

namespace spyro3 {

// SCUS_944.67's warp: from the idle title screen into the title's own start states. State 19
// fades and prepares, state 20 calls WarpToLevel 0x800584BC(0, level id) at 0x80077A50 in the
// title image.
class Warp final : public spyro::Warp {
public:
  // The 37 levels of levelIndexToHomeworldLevelId 0x800671A0 (homeworlds 10, 20 and 30 with nine
  // levels each, 40 with ten), named from the name table 0x80069CF8 in the same order.
  [[nodiscard]] static std::vector<spyro::WarpWorld> worlds(Core &core);

protected:
  // Game state 11 with the title image in state 1, the menu before the demo timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // The level id, then title state 19 from its first step.
  void startLevel(Core &core, std::uint32_t level) const override;
};

} // namespace spyro3
