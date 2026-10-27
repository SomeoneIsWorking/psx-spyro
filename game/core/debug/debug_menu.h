// debug_menu.h — the debug verbs Spyro 1/2/3 share: the level warp from the idle title screen and
// the moby teleport. Each title's menu derives from it and adds its own.
#pragma once

#include "title_debug_menu.h"

#include <cstdint>
#include <string>
#include <vector>

namespace spyro {

class MobyTeleport;

class DebugMenu : public psx::game::TitleDebugMenu {
public:
  // `warp <level>` over the ids `levelExists` accepts; `mobys` and `moby-teleport` when `mobys` is
  // given.
  DebugMenu(bool (*levelExists)(std::uint32_t), const MobyTeleport *mobys);

private:
  // The disc's level ids as the menu and the control line spell them.
  std::vector<std::string> levels_;
};

} // namespace spyro
