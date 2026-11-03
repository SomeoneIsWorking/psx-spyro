// debug_menu.h — Spyro 1's debug menu: the shared warp with each level's portal and crystal dragons
// as arrivals, the moby listing, and the portal and dragon listings.
#pragma once

#include "core/debug/debug_menu.h"
#include "spyro1/core/debug/teleport.h"

namespace spyro1 {

class DebugMenu final : public spyro::DebugMenu {
public:
  DebugMenu();

private:
  // The homeworlds, levels and arrivals from the executable's name and dragon tables.
  std::vector<spyro::WarpWorld> worlds(Core &core) const;

  teleport::LevelMobys mobys_;
};

} // namespace spyro1
