#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/warp.h"

#include <vector>

namespace spyro2 {

// SCUS_944.25's warp: from the idle title screen into the title's own start state, which tears the
// title down and calls 0x80019230 with the start index 0, keeping the level id already set.
class Warp final : public spyro::Warp {
public:
  // The 29 levels on the disc under their homeworlds (10, 30, 50), named from the name table
  // 0x800649B4 at the index the level-index table 0x80064940 gives each id.
  [[nodiscard]] static std::vector<spyro::WarpWorld> worlds(Core &core);

protected:
  // Game state 11 with the title in state 2, the "press start" screen before the demo timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // The level id, then title state 5 from its first tick, as state 4 enters it.
  void startLevel(Core &core, std::uint32_t level) const override;
  // No gameplay route into another level is traced yet (issue 0227); a level starts from the title
  // screen.
  [[nodiscard]] bool levelChangeReady(Core & /*core*/) const override {
    return false;
  }
  void changeLevel(Core & /*core*/, std::uint32_t /*level*/) const override {}
};

} // namespace spyro2
