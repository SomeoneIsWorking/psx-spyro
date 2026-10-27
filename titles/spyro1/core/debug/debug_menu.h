// debug_menu.h — Spyro 1's debug menu: the shared level warp and moby teleport, then the portal and
// crystal-dragon teleports.
#pragma once

#include "core/debug/debug_menu.h"
#include "spyro1/core/debug/teleport.h"

namespace spyro1 {

class DebugMenu final : public spyro::DebugMenu {
public:
  DebugMenu();

private:
  teleport::LevelMobys mobys_;
};

} // namespace spyro1
