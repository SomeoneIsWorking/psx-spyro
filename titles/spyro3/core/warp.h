#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/warp.h"

#include <vector>

namespace spyro3 {

// SCUS_944.67's warp: from the idle title screen into the title's own start states (state 19
// fades and prepares, state 20 calls WarpToLevel 0x800584BC(0, level id) at 0x80077A50 in the
// title image); from gameplay through WarpToLevel(0, level id) as the level-warp cheat's pause
// resume calls it (0x800583A4).
class Warp final : public spyro::Warp {
public:
  // The levels of LevelIdToLevelIndex 0x80066FCC under levelIndexToHomeworldLevelId 0x800671A0,
  // named from the name table 0x80069CF8; each level whose homeworld arrival the loader places at
  // its portal (UpdateLoadingImage 0x80058778) also arrives at that `portal`.
  [[nodiscard]] static std::vector<spyro::WarpWorld> worlds(Core &core);

protected:
  // Game state 11 with the title image in state 1, the menu before the demo timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // The level id, then title state 19 from its first step.
  void startLevel(Core &core, std::uint32_t level) const override;
  // Game state 0, free play in a loaded level.
  [[nodiscard]] bool levelChangeReady(Core &core) const override;
  void changeLevel(Core &core, std::uint32_t level) const override;
};

} // namespace spyro3
