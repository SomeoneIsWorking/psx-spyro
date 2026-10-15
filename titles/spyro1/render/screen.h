// screen.h — retail's outcode window, for the guest state SCUS_942.28's culls write: a
// widened draw leaves that state as retail would.
#pragma once

#include "render/terrain/guest_terrain_screen.h"
#include "spyro1/render/terrain_facts.h"

namespace spyro1 {

inline constexpr spyro::guest_terrain::ScreenBounds kRetailBounds{
    terrain::kSharedFacts.nativeWidth, 0, terrain::kSharedFacts.vertical};

} // namespace spyro1
