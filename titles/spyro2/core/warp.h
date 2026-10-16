#pragma once

#include "core/debug/warp.h"

namespace spyro2 {

// SCUS_944.25's warp: from the idle title screen into the title's own start state, which tears the
// title down and calls 0x80019230 with the start index 0, keeping the level id already set.
class Warp final : public spyro::Warp {
protected:
  // The ids the level-index table 0x80064940 maps to a level of the 29 on the disc.
  [[nodiscard]] bool levelExists(std::uint32_t level) const override;
  // Game state 11 with the title in state 2, the "press start" screen before the demo timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // The level id, then title state 5 from its first tick, as state 4 enters it.
  void startLevel(Core &core, std::uint32_t level) const override;
};

} // namespace spyro2
