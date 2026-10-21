// guest_terrain_fog.h — the distance fog the terrain drawer of this engine family gives a sector's
// vertex colours: when the level fogs (the fade level parked in the light matrix's L33 is non-zero)
// and the sector is not exempt, every colour word is DPCS-faded toward the level's fog colour into
// one shared buffer before the sector's polygons read them.
//
// The fog colour and that shared buffer are this image's addresses, so they arrive on the frame's
// facts rather than as constants here.
#pragma once

#include "core.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_facts.h"

#include <cstdint>

namespace spyro::guest_terrain {

// Load the level's fog colour (facts.fogColour: RFC, GFC, BFC words) into the GTE's far colour.
void loadFogColour(const psx::present::EmitMemory &memory, Core &core, const Facts &facts);

// Load the fog colour, then fade the colour words [colours, end - 4) by `fog` into
// facts.foggedColours (SCUS_944.25 800249A0, 80028DB0). Like retail, the loop reads the word at end
// - 4 into RGBC without fading it. Returns one past the last faded word written.
std::uint32_t fogColours(const psx::present::EmitMemory &memory,
                         Core &core,
                         const Facts &facts,
                         std::uint32_t colours,
                         std::uint32_t end,
                         std::uint32_t fog);

// A vertex colour between its near and far colour (SCUS_944.25 80024C94, SCUS_942.28 80026E18):
struct ColourFade {
  std::uint32_t nearColour = 0;
  std::uint32_t farColour = 0;
  std::uint32_t fade = 0;
};

// DPCS from RGBC = `farColour` toward FC = `nearColour` by IR0 = `fade`. The near colour's blue
// byte is loaded unmasked, as retail does.
std::uint32_t fadeColour(Core &core, const ColourFade &colours);

} // namespace spyro::guest_terrain
