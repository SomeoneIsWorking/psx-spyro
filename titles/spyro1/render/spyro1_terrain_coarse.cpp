// SCUS_942.28 80027688..80028590: the coarse subdivision pass. Each polygon the detail pass
// deferred coarse is cut 2x2 on the engine family's grid, its edges toward neighbours drawn whole
// closed with one crack triangle each, and its four pieces drawn from the split-texture record's
// own pairs.

#include "core.h"
#include "guest_terrain_grid.h"
#include "guest_terrain_polygon.h"
#include "spyro1_terrain_split.h"

#include <array>

namespace spyro1::terrain {
namespace {

namespace grid = spyro::guest_terrain::grid;
namespace packet = spyro::guest_terrain::packet;
namespace cell = spyro::guest_terrain::cell;
using spyro::guest_terrain::GridPoint;
using spyro::guest_terrain::kCellSize;
using spyro::guest_terrain::kDepthMask;
using spyro::guest_terrain::kOutcodeMask;
using spyro::guest_terrain::kScratchpad;
using spyro::guest_terrain::kSplitPrimitiveCursor;
using spyro::guest_terrain::PackedIndices;
using spyro::guest_terrain::PacketSize;

constexpr std::uint32_t kGt4ToGt3 = 0x08000000u;
constexpr std::uint32_t kNeighbourDrawnWhole = 1;
constexpr std::uint32_t kPieceUvSpan = 0x1F;
constexpr std::uint32_t kQuadPieces = 4;
constexpr std::uint32_t kTrianglePieces = 4;

class CoarsePass final : public Spyro1SplitPass {
public:
  CoarsePass(TerrainFrame &frame, const Facts &facts) : Spyro1SplitPass(frame, facts) {}

  void polygon(const SplitSector &sector, const SplitPolygon &polygon) override;

private:
  void quad(const SplitSector &sector, const SplitRecord &record);
  void triangle(const SplitSector &sector, const SplitRecord &record);
};

void CoarsePass::polygon(const SplitSector &sector, const SplitPolygon &polygon) {
  const SplitRecord split = record(polygon);
  if (polygon.triangle) {
    triangle(sector, split);
  } else {
    quad(sector, split);
  }
}

// 80027750.
void CoarsePass::quad(const SplitSector &sector, const SplitRecord &record) {
  const PackedIndices vertices(record.indices);
  const std::array<GridPoint, 4> corners = {unpackGridVertex(frame_, sector, vertices.first),
                                            unpackGridVertex(frame_, sector, vertices.second),
                                            unpackGridVertex(frame_, sector, vertices.fourth),
                                            unpackGridVertex(frame_, sector, vertices.third)};
  grid::storeCoarseQuad(frame_, corners, quadColours(sector, record));
  projectGrid(frame_, grid::kCoarseQuadCells, false);
  grid::refineCoarseCentre(frame_);
  grid::drawEdgeCracks(
      frame_,
      {.edges = quadEdges(record, kNeighbourDrawnWhole), .table = facts_.coarseQuadCracks},
      crack(record, kGt4ToGt3));

  // 80027D7C: the four GT4s.
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kQuadPieces; ++k) {
    const std::uint32_t first = memory.r32(facts_.coarseQuadCells + 4 * k);
    frame_.v1 = first;
    const std::array<std::uint32_t, 4> cells = {
        first, first + kCellSize, first + 3 * kCellSize, first + 4 * kCellSize};
    std::array<std::uint32_t, 4> codes{};
    std::uint32_t shared = 0xFFFFFFFFu;
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < cells.size(); ++i) {
      codes[i] = memory.r32(cells[i] + cell::kDepth);
      shared &= codes[i];
      sum += codes[i] & kDepthMask;
    }
    if ((shared & kOutcodeMask) != 0) {
      continue;
    }
    std::array<std::uint32_t, 4> screens{};
    for (std::size_t i = 0; i < cells.size(); ++i) {
      screens[i] = memory.r32(cells[i] + cell::kScreen);
    }
    const std::uint32_t prim = frame_.primitive;
    if (record.overflow && !facesCamera(frame_,
                                        screens,
                                        sidedness(record.draw),
                                        spyro::guest_terrain::SecondTriangle::Reversed)) {
      memory.w32(prim + packet::kXy0, screens[0]);
      continue;
    }
    memory.w32(prim + packet::kXy0, screens[0]);
    memory.w32(prim + packet::kXy1, screens[1]);
    memory.w32(prim + packet::kXy2, screens[2]);
    memory.w32(prim + packet::kXy3, screens[3]);
    memory.w32(prim + packet::kColour0, memory.r32(cells[0] + cell::kColour));
    memory.w32(prim + packet::kColour1, memory.r32(cells[1] + cell::kColour));
    memory.w32(prim + packet::kColour2, memory.r32(cells[2] + cell::kColour));
    memory.w32(prim + packet::kColour3, memory.r32(cells[3] + cell::kColour));
    writePieceUvs({.pair = record.texture + kCoarsePairs + 8 * k, .span = kPieceUvSpan});
    memory.w32(prim, spyro::guest_terrain::kQuadTag);
    frame_.v1 = frame_
                    .linkAndAdvance((sum >> 7) + binBias(record.draw),
                                    PacketSize{spyro::guest_terrain::kQuadBytes})
                    .head;
  }
  frame_.v0 = facts_.coarseQuadCells + 4 * kQuadPieces;
}

// 80027F28.
void CoarsePass::triangle(const SplitSector &sector, const SplitRecord &record) {
  const PackedIndices vertices(record.indices);
  const std::array<GridPoint, 3> corners = {unpackGridVertex(frame_, sector, vertices.first),
                                            unpackGridVertex(frame_, sector, vertices.second),
                                            unpackGridVertex(frame_, sector, vertices.third)};
  grid::storeCoarseTriangle(frame_, corners, triangleColours(sector, record));
  projectGrid(frame_, grid::kCoarseTriangleCells, false);
  const std::uint32_t corner = record.links & 0x300u;
  grid::drawEdgeCracks(frame_,
                       {.edges = triangleEdges(record, kNeighbourDrawnWhole),
                        .table = facts_.coarseTriangleCracks + (corner >> 3) + (corner >> 6)},
                       crack(record, 0));

  // 800283DC: the four GT3s.
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kTrianglePieces; ++k) {
    const std::uint32_t layout = memory.r32(facts_.coarseTriangleCells + 4 * k);
    writeTrianglePieceUvs(
        record,
        layout,
        {.subTextures = facts_.coarseTriangleSubTexture, .selectMask = 0xC, .pairs = kCoarsePairs});
    const std::array<std::uint32_t, 3> cells = {kScratchpad + ((layout >> 20) & 0xFF0u),
                                                kScratchpad + ((layout >> 12) & 0xFF0u),
                                                kScratchpad + ((layout >> 4) & 0xFF0u)};
    const std::uint32_t d0 = memory.r32(cells[0] + cell::kDepth);
    const std::uint32_t d1 = memory.r32(cells[1] + cell::kDepth);
    const std::uint32_t d2 = memory.r32(cells[2] + cell::kDepth);
    frame_.v0 = d1;
    frame_.v1 = d2;
    if ((d0 & d1 & d2 & kOutcodeMask) != 0) {
      continue;
    }
    const std::uint32_t sum = (d0 & kDepthMask) + (d1 & kDepthMask) + 2 * (d2 & kDepthMask);
    const std::array<std::uint32_t, 3> screens = {memory.r32(cells[0] + cell::kScreen),
                                                  memory.r32(cells[1] + cell::kScreen),
                                                  memory.r32(cells[2] + cell::kScreen)};
    frame_.v0 = screens[1];
    frame_.v1 = screens[2];
    const std::uint32_t prim = frame_.primitive;
    if (record.overflow && !triangleFacesCamera(frame_, screens, sidedness(record.draw))) {
      memory.w32(prim + packet::kXy0, screens[0]);
      continue;
    }
    memory.w32(prim + packet::kXy0, screens[0]);
    memory.w32(prim + packet::kXy1, screens[1]);
    memory.w32(prim + packet::kXy2, screens[2]);
    memory.w32(prim + packet::kColour0, memory.r32(cells[0] + cell::kColour));
    memory.w32(prim + packet::kColour1, memory.r32(cells[1] + cell::kColour));
    memory.w32(prim + packet::kColour2, memory.r32(cells[2] + cell::kColour));
    memory.w32(prim, spyro::guest_terrain::kTriangleTag);
    frame_.v0 = frame_
                    .linkAndAdvance((sum >> 7) + binBias(record.draw),
                                    PacketSize{spyro::guest_terrain::kTriangleBytes})
                    .head;
    frame_.v1 = prim >> 16;
  }
}

} // namespace

void splitCoarsePolygons(TerrainFrame &frame, const Facts &facts) {
  CoarsePass pass(frame, facts);
  walkSplitList(frame, frame.scratch + kCoarseSplitList, pass);
  // 8002857C: the re-split queue starts where the coarse list did.
  frame.memory.w32(kSplitPrimitiveCursor, frame.scratch + frame.facts.resplit.queue);
}

} // namespace spyro1::terrain
