// guest_terrain_grid.h — the grids the split passes of this engine family cut a deferred polygon
// into, filled the same way by every image: coordinates by halving, colours by `blend`, and the
// inner points pulled back toward their plane after projection (guest_terrain_split.h has the cell
// layout).
//
// Corners arrive in grid order: a quad's first, second, fourth and third vertex (its 0, 1, 3, 2
// winding), a triangle's first, second and third.
#pragma once

#include "guest_terrain_frame.h"
#include "guest_terrain_split.h"

#include <array>
#include <cstdint>

namespace spyro::guest_terrain {

namespace grid {

inline constexpr std::uint32_t kCoarseQuadCells = 9;     // 3x3, corners 0 2 6 8
inline constexpr std::uint32_t kCoarseTriangleCells = 6; // 0 1 2 / 3 4 / 5
inline constexpr std::uint32_t kFineQuadCells = 25;      // 5x5, corners 0 4 20 24
inline constexpr std::uint32_t kFineTriangleCells = 15;  // rows of 5 4 3 2 1

[[nodiscard]] inline std::uint32_t cellAddress(std::uint32_t index) {
  return kScratchpad + index * kCellSize;
}

// The coarse grids' positions and colours; the quad's centre colour blends the second and fourth
// corners, a diagonal, where its position averages all four.
void storeCoarseQuad(TerrainFrame &frame,
                     const std::array<GridPoint, 4> &corners,
                     const std::array<std::uint32_t, 4> &colours);
void storeCoarseTriangle(TerrainFrame &frame,
                         const std::array<GridPoint, 3> &corners,
                         const std::array<std::uint32_t, 3> &colours);

// The fine grids; the quad's colour centre is the same diagonal blend and its inner ring blends
// toward it.
void storeFineQuad(TerrainFrame &frame,
                   const std::array<GridPoint, 4> &corners,
                   const std::array<std::uint32_t, 4> &colours);
void storeFineTriangle(TerrainFrame &frame,
                       const std::array<GridPoint, 3> &corners,
                       const std::array<std::uint32_t, 3> &colours);

// A projected coarse quad's centre past depth 0x700 is pulled toward the midpoint of the diagonal
// its colour was blended from.
void refineCoarseCentre(TerrainFrame &frame);

// When every fine quad corner is past depth 0x40, the four inner diagonal points (cell addresses
// in the image's table at `innerPoints`) are pulled toward the points a row above and below.
void refineFineInnerPoints(TerrainFrame &frame, std::uint32_t innerPoints);

// One crack triangle per edge whose neighbour byte is set, from consecutive 12-byte entries.
void drawEdgeCracks(TerrainFrame &frame, CrackEdges edges, const CrackPolygon &polygon);

} // namespace grid
} // namespace spyro::guest_terrain
