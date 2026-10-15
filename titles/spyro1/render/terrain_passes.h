// terrain_passes.h — the passes of SCUS_942.28's terrain drawer (0x800258F0), in the order
// it runs them. The re-split pass is the engine family's shared one (guest_terrain_passes.h).
#pragma once

#include "render/terrain/guest_terrain_frame.h"
#include "spyro1/render/terrain_facts.h"

#include <cstdint>

namespace spyro1::terrain {

using spyro::guest_terrain::TerrainFrame;

// The sector and split lists, as offsets in the scratch block. The coarse split list reuses the far
// list once the far pass has walked it, and the re-split queue reuses it in turn.
inline constexpr std::uint32_t kNearList = 0x1C00;
inline constexpr std::uint32_t kFineSplitList = 0x1E00;
inline constexpr std::uint32_t kFarList = 0x2000;
inline constexpr std::uint32_t kCoarseSplitList = 0x2000;

// 80025928: which sectors are drawn, and by which pass. `group` is the drawer's $a0: a group's
// byte list, or every sector when negative.
void listSectors(TerrainFrame &frame, const Facts &facts, std::int32_t group);
// 80026200: the far sectors as untextured Gouraud polygons.
void drawFarSectors(TerrainFrame &frame, const Facts &facts);
// 80026788: the near sectors as textured polygons, deferring the nearest to the split lists.
void drawDetailSectors(TerrainFrame &frame, const Facts &facts);
// 80027688: polygons deferred coarse, cut 2x2.
void splitCoarsePolygons(TerrainFrame &frame, const Facts &facts);
// 80028590: polygons deferred fine, cut 4x4.
void splitFinePolygons(TerrainFrame &frame, const Facts &facts);

} // namespace spyro1::terrain
