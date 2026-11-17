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

// Town Square (level 13), `warp 0/3/0`.
// grows: 6 lawn, plain green (0x801C (160,128)); 5 lawn with yellow specks (0x801C (128,128));
// 11 lawn, plain green (0x801D (64,128)). Not 25, 27 or 9: grass fading into stone.
// paths: 28 pink cobbles; 51, 50, 60, 59 sandstone slabs; 49 sand.
inline constexpr std::array<std::uint8_t, 3> kTownSquareGrows{6, 5, 11};
inline constexpr std::array<std::uint8_t, 6> kTownSquarePaths{28, 51, 50, 60, 59, 49};

// Magic Crafters (level 30), `warp 2/0/0`.
// grows: 17 lawn, plain green (0x8019 (192,192)); 57 lawn with yellow specks (0x800B (160,64)).
// Not 33, 35, 31, 34 or 36: lawn with snow at its edges.
// paths: 45 grey stone; 37 checker tiles; 58 pale stone floor.
inline constexpr std::array<std::uint8_t, 2> kMagicCraftersGrows{17, 57};
inline constexpr std::array<std::uint8_t, 3> kMagicCraftersPaths{45, 37, 58};

// Dream Weavers (level 50), `warp 4/0/0`.
// grows: 43 lawn, plain green (0x800F (64,64)); 10 lawn with pale specks (0x8009 (192,64)).
// Not 11: lawn fading into cloud.
// paths: 21 grey stone; 37 pink brick.
inline constexpr std::array<std::uint8_t, 2> kDreamWeaversGrows{43, 10};
inline constexpr std::array<std::uint8_t, 2> kDreamWeaversPaths{21, 37};

inline constexpr std::array<spyro::guest_terrain::VegetationLevel, 4> kVegetationLevels{{
    {.level = 11, .grows = kStoneHillGrows, .paths = kStoneHillPaths},
    {.level = 13, .grows = kTownSquareGrows, .paths = kTownSquarePaths},
    {.level = 30, .grows = kMagicCraftersGrows, .paths = kMagicCraftersPaths},
    {.level = 50, .grows = kDreamWeaversGrows, .paths = kDreamWeaversPaths},
}};

} // namespace spyro1::terrain
