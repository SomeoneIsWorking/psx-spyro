#include "guest_terrain_grid.h"

#include "guest_terrain_frame.h"
#include "guest_word.h"

#include <cstddef>

namespace spyro::guest_terrain::grid {
namespace {

using spyro::guest_word::asSigned;

constexpr std::uint32_t kCentreSplitDepth = 0x700;
constexpr std::uint32_t kCentreSplitSpan = 0x100;
constexpr std::uint32_t kInnerRefineDepth = 0x40;
constexpr std::uint32_t kInnerRefineSpan = 0x100;
constexpr std::uint32_t kCrackEntrySize = 12;

// One coordinate (or colour) over the 5x5 grid.
template <typename Average>
std::array<std::uint32_t, kFineQuadCells> quadGrid(const std::array<std::uint32_t, 4> &corners,
                                                   Average average) {
  const auto [a, b, c, d] = corners;
  std::array<std::uint32_t, kFineQuadCells> g{};
  g[0] = a;
  g[4] = b;
  g[20] = c;
  g[24] = d;
  g[2] = average(a, b);
  g[1] = average(a, g[2]);
  g[3] = average(b, g[2]);
  g[22] = average(c, d);
  g[21] = average(c, g[22]);
  g[23] = average(d, g[22]);
  g[10] = average(a, c);
  g[14] = average(b, d);
  g[12] = average(g[10], g[14]);
  g[11] = average(g[10], g[12]);
  g[13] = average(g[14], g[12]);
  g[5] = average(a, g[10]);
  g[9] = average(b, g[14]);
  g[15] = average(c, g[10]);
  g[19] = average(d, g[14]);
  g[7] = average(g[5], g[9]);
  g[6] = average(g[5], g[7]);
  g[8] = average(g[9], g[7]);
  g[17] = average(g[15], g[19]);
  g[16] = average(g[15], g[17]);
  g[18] = average(g[19], g[17]);
  return g;
}

// One coordinate over the 15-point triangle grid.
template <typename Average>
std::array<std::uint32_t, kFineTriangleCells>
triangleGrid(std::uint32_t a, std::uint32_t b, std::uint32_t c, Average average) {
  std::array<std::uint32_t, kFineTriangleCells> g{};
  g[0] = a;
  g[4] = b;
  g[14] = c;
  g[2] = average(a, b);
  g[1] = average(a, g[2]);
  g[3] = average(b, g[2]);
  g[9] = average(a, c);
  g[5] = average(a, g[9]);
  g[12] = average(c, g[9]);
  g[11] = average(b, c);
  g[8] = average(b, g[11]);
  g[13] = average(c, g[11]);
  g[6] = average(a, g[11]);
  g[7] = average(b, g[9]);
  g[10] = average(c, g[2]);
  return g;
}

std::uint32_t coordinateMidpoint(std::uint32_t a, std::uint32_t b) {
  return midpoint(a, b);
}

std::uint32_t colourMidpoint(std::uint32_t a, std::uint32_t b) {
  return blend(a, b);
}

template <std::size_t N>
void storeComponent(guest_memory::WorkingMemory &memory,
                    std::uint32_t offset,
                    const std::array<std::uint32_t, N> &values) {
  for (std::size_t i = 0; i < N; ++i) {
    memory.w16(cellAddress(static_cast<std::uint32_t>(i)) + offset,
               static_cast<std::uint16_t>(values[i]));
  }
}

template <std::size_t N>
void storeColours(guest_memory::WorkingMemory &memory, const std::array<std::uint32_t, N> &values) {
  for (std::size_t i = 0; i < N; ++i) {
    memory.w32(cellAddress(static_cast<std::uint32_t>(i)) + cell::kColour, values[i]);
  }
}

void setColour(TerrainFrame &frame, std::uint32_t index, std::uint32_t colour) {
  frame.memory.w32(cellAddress(index) + cell::kColour, colour);
}

} // namespace

void storeCoarseQuad(TerrainFrame &frame,
                     const std::array<GridPoint, 4> &corners,
                     const std::array<std::uint32_t, 4> &colours) {
  const auto [p0, p1, p3, p2] = corners;
  storeGridPoint(frame, 0, p0);
  storeGridPoint(frame, 2, p1);
  storeGridPoint(frame, 6, p3);
  storeGridPoint(frame, 8, p2);
  storeGridPoint(frame, 1, midpoint(p0, p1));
  storeGridPoint(frame, 7, midpoint(p3, p2));
  const GridPoint left = midpoint(p0, p3);
  const GridPoint right = midpoint(p1, p2);
  storeGridPoint(frame, 3, left);
  storeGridPoint(frame, 5, right);
  storeGridPoint(frame, 4, midpoint(left, right));
  setColour(frame, 0, colours[0]);
  setColour(frame, 2, colours[1]);
  setColour(frame, 6, colours[2]);
  setColour(frame, 8, colours[3]);
  setColour(frame, 1, blend(colours[0], colours[1]));
  setColour(frame, 7, blend(colours[2], colours[3]));
  setColour(frame, 3, blend(colours[0], colours[2]));
  setColour(frame, 5, blend(colours[1], colours[3]));
  setColour(frame, 4, blend(colours[1], colours[2]));
}

void storeCoarseTriangle(TerrainFrame &frame,
                         const std::array<GridPoint, 3> &corners,
                         const std::array<std::uint32_t, 3> &colours) {
  const auto [p0, p1, p2] = corners;
  storeGridPoint(frame, 0, p0);
  storeGridPoint(frame, 2, p1);
  storeGridPoint(frame, 5, p2);
  storeGridPoint(frame, 1, midpoint(p0, p1));
  storeGridPoint(frame, 3, midpoint(p0, p2));
  storeGridPoint(frame, 4, midpoint(p1, p2));
  setColour(frame, 0, colours[0]);
  setColour(frame, 2, colours[1]);
  setColour(frame, 5, colours[2]);
  setColour(frame, 1, blend(colours[0], colours[1]));
  setColour(frame, 3, blend(colours[0], colours[2]));
  setColour(frame, 4, blend(colours[1], colours[2]));
}

void storeFineQuad(TerrainFrame &frame,
                   const std::array<GridPoint, 4> &corners,
                   const std::array<std::uint32_t, 4> &colours) {
  const auto [a, b, c, d] = corners;
  storeComponent(frame.memory, cell::kZ, quadGrid({a.z, b.z, c.z, d.z}, coordinateMidpoint));
  storeComponent(frame.memory, cell::kX, quadGrid({a.x, b.x, c.x, d.x}, coordinateMidpoint));
  storeComponent(frame.memory, cell::kY, quadGrid({a.y, b.y, c.y, d.y}, coordinateMidpoint));
  std::array<std::uint32_t, kFineQuadCells> grid = quadGrid(colours, colourMidpoint);
  grid[12] = blend(colours[1], colours[2]);
  grid[7] = blend(grid[12], grid[2]);
  grid[11] = blend(grid[12], grid[10]);
  grid[13] = blend(grid[12], grid[14]);
  grid[17] = blend(grid[12], grid[22]);
  grid[6] = blend(colours[0], grid[12]);
  grid[8] = blend(colours[1], grid[12]);
  grid[16] = blend(colours[2], grid[12]);
  grid[18] = blend(colours[3], grid[12]);
  storeColours(frame.memory, grid);
}

void storeFineTriangle(TerrainFrame &frame,
                       const std::array<GridPoint, 3> &corners,
                       const std::array<std::uint32_t, 3> &colours) {
  const auto [a, b, c] = corners;
  storeComponent(frame.memory, cell::kZ, triangleGrid(a.z, b.z, c.z, coordinateMidpoint));
  storeComponent(frame.memory, cell::kX, triangleGrid(a.x, b.x, c.x, coordinateMidpoint));
  storeComponent(frame.memory, cell::kY, triangleGrid(a.y, b.y, c.y, coordinateMidpoint));
  storeColours(frame.memory, triangleGrid(colours[0], colours[1], colours[2], colourMidpoint));
}

void refineCoarseCentre(TerrainFrame &frame) {
  const std::uint32_t centre = cellAddress(4);
  const std::uint32_t code = frame.memory.r32(centre + cell::kDepth);
  if ((code & kOutcodeMask) != 0) {
    return;
  }
  const std::uint32_t past = (code & kDepthMask) - kCentreSplitDepth;
  if (asSigned(past) <= 0) {
    return;
  }
  pullTowardPair(frame,
                 {.point = centre,
                  .first = cellAddress(2),
                  .second = cellAddress(6),
                  .ownWeight = kCentreSplitSpan - past,
                  .pairWeight = past >> 1});
}

void refineFineInnerPoints(TerrainFrame &frame, std::uint32_t innerPoints) {
  constexpr std::array<std::uint32_t, 4> kCorners = {0, 4, 20, 24};
  std::array<std::uint32_t, 4> past{};
  for (std::size_t i = 0; i < kCorners.size(); ++i) {
    past[i] = (frame.memory.r32(cellAddress(kCorners[i]) + cell::kDepth) & kDepthMask) -
              kInnerRefineDepth;
    if (asSigned(past[i]) <= 0) {
      return;
    }
  }
  std::uint32_t nearest = past[0];
  for (std::size_t i = 1; i < past.size(); ++i) {
    if (asSigned(past[i] - nearest) < 0) {
      nearest = past[i];
    }
  }
  for (std::uint32_t k = 0; k < 4; ++k) {
    const std::uint32_t point = frame.memory.r32(innerPoints + 4 * k);
    if ((frame.memory.r32(point + cell::kDepth) & kOutcodeMask) != 0) {
      continue;
    }
    pullTowardPair(frame,
                   {.point = point,
                    .first = point - 4 * kCellSize,
                    .second = point + 4 * kCellSize,
                    .ownWeight = kInnerRefineSpan - nearest,
                    .pairWeight = nearest >> 1});
  }
}

void drawEdgeCracks(TerrainFrame &frame, CrackEdges edges, const CrackPolygon &polygon) {
  std::uint32_t entry = edges.table;
  std::uint32_t remaining = edges.edges;
  while (remaining != 0) {
    const std::uint32_t edge = remaining & 0xFFu;
    remaining >>= 8;
    entry += kCrackEntrySize;
    if (edge != 0) {
      emitCrackTriangle(frame, entry - kCrackEntrySize, polygon);
    }
  }
}

} // namespace spyro::guest_terrain::grid
