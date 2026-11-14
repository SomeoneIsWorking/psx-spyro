// vegetation_table.h — Spyro 3's reviewed vegetation tables (G006): per level, the near texture
// records whose faces grow added grass and the path records nothing grows beside. Each record was
// judged on its full-bright 32x32 crop from tools/vegetation_listing.py (page, uv, clut noted).
#pragma once

#include "render/terrain/terrain_vegetation.h"

#include <array>
#include <cstdint>

namespace spyro3 {

// Sunny Villa (level 11), `warp 0/1/0`.
// grows: 49 bright lawn (0x001F (224,128)); 19 lawn with a flower (0x001C (32,128)); 20 lawn with
// yellow specks (0x001C (64,128)). Not grown: 46 and the teal edges 21-29 (hedge tops); 10, 11, 47,
// 48 grass over stone walls. paths: 74, 70 plaster floor; 15-18 stepping stones in grass; 45 wood;
// 82 stone.
inline constexpr std::array<std::uint8_t, 3> kSunnyVillaGrows{49, 19, 20};
inline constexpr std::array<std::uint8_t, 8> kSunnyVillaPaths{74, 70, 15, 16, 17, 18, 45, 82};

inline constexpr std::array<spyro::guest_terrain::VegetationLevel, 1> kVegetationLevels{{
    {.level = 11, .grows = kSunnyVillaGrows, .paths = kSunnyVillaPaths},
}};

} // namespace spyro3
