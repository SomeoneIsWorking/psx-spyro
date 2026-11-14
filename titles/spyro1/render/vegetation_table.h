// vegetation_table.h — Spyro 1's reviewed vegetation tables (G006): per level, the near texture
// records whose faces grow added grass and the path records nothing grows beside. Each record was
// judged on its full-bright 32x32 crop from tools/vegetation_listing.py (page, uv, clut noted).
#pragma once

#include "render/terrain/terrain_vegetation.h"

#include <array>
#include <cstdint>

namespace spyro1::terrain {

// Stone Hill (level 11), `warp 0/1/0`.
// grows: 40 lawn, plain green (0x8018 (224,160)); 4 lawn with yellow specks (0x8018 (128,128)).
// paths: 11 brown dirt path; 30 sand; 15, 18 grey stone slabs.
inline constexpr std::array<std::uint8_t, 2> kStoneHillGrows{40, 4};
inline constexpr std::array<std::uint8_t, 4> kStoneHillPaths{11, 30, 15, 18};

inline constexpr std::array<spyro::guest_terrain::VegetationLevel, 1> kVegetationLevels{{
    {.level = 11, .grows = kStoneHillGrows, .paths = kStoneHillPaths},
}};

} // namespace spyro1::terrain
