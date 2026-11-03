#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/warp.h"

#include <optional>
#include <vector>

namespace spyro2 {

// SCUS_944.25's warp: from the idle title screen into the title's own start state, which tears the
// title down and calls 0x80019230 with the start index 0, keeping the level id already set; from
// gameplay through the pause menu's Exit Level (0x80019928(-1)), its fade and load aimed at the
// chosen level. The game leaves a level only for its own homeworld, so a homeworld is reached
// through its first level and that level's exit, arriving in front of its portal.
class Warp final : public spyro::Warp {
public:
  // The levels of the level-index table 0x80064940 under the homeworld 0x80064B84 gives each
  // index, named from the name table 0x800649B4; each level a homeworld portal leads to also
  // arrives at that `portal`.
  [[nodiscard]] static std::vector<spyro::WarpWorld> worlds(Core &core);

protected:
  // Game state 11 with the title in state 2, the "press start" screen before the demo timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // The level id, then title state 5 from its first tick, as state 4 enters it.
  void startLevel(Core &core, std::uint32_t level) const override;
  // Game state 0, free play in a loaded level.
  [[nodiscard]] bool levelChangeReady(Core &core) const override;
  // Exit Level, then its destination 0x8006715C set to the level, or a homeworld's first level.
  void changeLevel(Core &core, std::uint32_t level) const override;
  // For a homeworld, that first level's exit.
  [[nodiscard]] std::optional<Placement> afterLevelChange(Core &core,
                                                          std::uint32_t level) const override;
};

} // namespace spyro2
