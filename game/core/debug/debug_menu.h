// debug_menu.h — the debug verbs Spyro 1/2/3 share: the level warp from the idle title screen and
// the moby teleport. Each title's menu derives from it and adds its own.
#pragma once

#include "core/frame_cut.h"
#include "title_debug_menu.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace spyro {

class MobyTeleport;

// The game states a screen route can reach: gameplay and the pause menu over it by warping, the
// title screen by starting the title.
struct ScreenStates {
  std::uint32_t playing = 0;
  std::uint32_t title = 0;
  std::optional<std::uint32_t> pause;
};

class DebugMenu : public psx::game::TitleDebugMenu {
public:
  // `warp <level>` over the ids `levelExists` accepts; `mobys` and `moby-teleport` when `mobys` is
  // given. A screen is the game state and level id `scene` names; `states` says which a route
  // reaches.
  DebugMenu(bool (*levelExists)(std::uint32_t),
            const MobyTeleport *mobys,
            const FrameCutFacts &scene,
            const ScreenStates &states);

private:
  // The disc's level ids as the menu and the control line spell them.
  std::vector<std::string> levels_;
};

} // namespace spyro
