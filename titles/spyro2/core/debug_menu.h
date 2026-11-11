// debug_menu.h — Spyro 2's debug menu: the shared level warp and moby teleport over its moby array.
#pragma once

#include "core/debug/debug_menu.h"
#include "core/debug/moby_teleport.h"

namespace spyro2 {

class DebugMenu final : public spyro::DebugMenu {
public:
  DebugMenu();

private:
  spyro::RecordMobyTeleport mobys_;
};

} // namespace spyro2
