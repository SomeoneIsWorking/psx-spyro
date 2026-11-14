// vegetation_table.h — Spyro 2's reviewed vegetation tables (G006): per level, the near texture
// records whose faces grow added grass and the path records nothing grows beside. Each record was
// judged on its full-bright 32x32 crop from tools/vegetation_listing.py (page, uv, clut noted).
#pragma once

#include "render/terrain/terrain_vegetation.h"

#include <array>
#include <cstdint>

namespace spyro2 {

// Summer Forest (level 10), `warp 0/0/0`.
// grows: 59 bright lawn (0x001E (192,192)); 41 dark mossy lawn (0x001D (160,192)).
// Not grown: 49, 85, 86 leaf canopy (hedges, tree tops); 39-46 grass-to-dirt edges.
// paths: 63, 66 gold carpet; 68, 69, 67 cobbles; 44 bare dirt; 84, 83 gravel.
inline constexpr std::array<std::uint8_t, 2> kSummerForestGrows{59, 41};
inline constexpr std::array<std::uint8_t, 8> kSummerForestPaths{63, 66, 68, 69, 67, 44, 84, 83};

inline constexpr std::array<spyro::guest_terrain::VegetationLevel, 1> kVegetationLevels{{
    {.level = 10, .grows = kSummerForestGrows, .paths = kSummerForestPaths},
}};

} // namespace spyro2
