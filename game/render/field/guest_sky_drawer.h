// guest_sky_drawer.h — the sky drawer Spyro 2 and 3 share (SCUS_944.25 0x80029170, SCUS_944.67
// 0x8001C368, the same instructions): every sky sector the visibility leaf marks and whose sphere
// reaches in front of the eye is projected and outcoded, and its triangle strips are chained as
// POLY_F3/POLY_G3 into the bin one past the mark. Native so its outcodes test the presented window:
// retail's x tests at 0 and 512 drop every sky polygon wholly in a 16:9 margin.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"

#include <cstdint>

namespace spyro::guest_sky {

struct Facts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // Its `jal` to the terrain's sector-visibility leaf, which with $a0 = 1 marks the sky sectors.
  std::uint32_t visibilityCallSite = 0;
  // The sky sector count, then the sector pointer table.
  std::uint32_t sectors = 0;
};

// The drawer's body, from its prologue to its epilogue.
void draw(Core &core,
          const Facts &sky,
          const guest_terrain::Facts &terrain,
          const guest_render_globals::Globals &globals);

} // namespace spyro::guest_sky
