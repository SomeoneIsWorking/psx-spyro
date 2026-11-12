#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/warp.h"

#include <vector>

namespace spyro2 {

// SCUS_944.25's warp: from the idle title screen into the title's own start state, which tears the
// title down and calls 0x80019230 with the start index 0, keeping the level id already set; from
// gameplay through the pause menu's Exit Level (0x80019928(-1)), its fade and load aimed at the
// chosen level. A homeworld is entered from one of its own levels, arriving in front of that
// level's portal, and a boss arena from its homeworld through the boss door (0x8001A290). The first
// exit from Glimmer plays the game's story scene into Summer Forest (0x80071D98), and the route
// continues from there.
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
  // From the level's homeworld, through its portal there (0x80019928(index)); from elsewhere, Exit
  // Level with its destination 0x8006715C set to the level; a boss arena through the boss door
  // from its homeworld.
  void changeLevel(Core &core, std::uint32_t level) const override;
  // Game state 0 in `level`.
  [[nodiscard]] bool playing(Core &core, std::uint32_t level) const override;

private:
  // From a level of `homeworld`, Exit Level as the game takes it; from elsewhere, Exit Level into
  // its first level with a portal.
  void enterHomeworld(Core &core, std::uint32_t homeworld) const;
};

} // namespace spyro2
