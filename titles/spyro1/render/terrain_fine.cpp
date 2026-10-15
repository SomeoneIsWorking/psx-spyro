// SCUS_942.28 80028590..8002A0A0: the fine subdivision pass. Quads (first walk) and then triangles
// (second walk) the detail pass deferred fine are cut 4x4, edges toward coarser neighbours closed
// with two crack triangles (three toward one drawn whole), and each of the sixteen pieces drawn
// from its own pair of the split-texture record. A piece too large for the GPU is also queued for
// the re-split pass.

#include "core.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_grid.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "spyro1/render/terrain_split.h"

#include <array>
#include <span>

namespace spyro1::terrain {
namespace {

namespace grid = spyro::guest_terrain::grid;
namespace packet = spyro::guest_terrain::packet;
namespace cell = spyro::guest_terrain::cell;
using spyro::guest_terrain::gpuSpan;
using spyro::guest_terrain::GpuSpan;
using spyro::guest_terrain::GridPoint;
using spyro::guest_terrain::kCellSize;
using spyro::guest_terrain::kDepthMask;
using spyro::guest_terrain::kOutcodeMask;
using spyro::guest_terrain::kScratchpad;
using spyro::guest_terrain::kSplitPrimitiveCursor;
using spyro::guest_terrain::PackedIndices;
using spyro::guest_terrain::PacketSize;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kGt4ToGt3 = 0x08000000u;
constexpr std::uint32_t kNeighbourCoarser = 3; // drawn whole or deferred coarse
constexpr std::uint32_t kNeighbourDrawnWhole = 1;
constexpr std::uint32_t kCrackEntrySize = 12;
constexpr std::uint32_t kCracksPerEdge = 3;
constexpr std::uint32_t kTriangleCrackCorner = 0x6C;
constexpr std::uint32_t kPieceUvSpan = 0xF;
constexpr std::uint32_t kPieces = 16;
constexpr std::uint32_t kRowStride = 5 * kCellSize;

class FinePass final : public SplitPass {
public:
  FinePass(TerrainFrame &frame, const Facts &facts, bool triangles)
      : SplitPass(frame, facts), triangles_(triangles) {}

  void polygon(const SplitSector &sector, const SplitPolygon &polygon) override;

private:
  void quad(const SplitSector &sector, const SplitRecord &record);
  void triangle(const SplitSector &sector, const SplitRecord &record);
  void drawCracks(spyro::guest_terrain::CrackEdges edges,
                  const spyro::guest_terrain::CrackPolygon &polygon);
  void drawQuadPieces(const SplitRecord &record);
  void drawTrianglePieces(const SplitRecord &record);
  // Queue the piece for the re-split pass when too large; false when its depths sum to zero and
  // it is not linked.
  bool queue(GpuSpan span, std::uint32_t sum);

  bool triangles_;
};

void FinePass::polygon(const SplitSector &sector, const SplitPolygon &polygon) {
  if (polygon.triangle != triangles_) {
    return;
  }
  const SplitRecord split = record(polygon);
  if (polygon.triangle) {
    triangle(sector, split);
  } else {
    quad(sector, split);
  }
}

// 80028ED8: two crack triangles per edge whose neighbour is coarser, and a third where it was
// drawn whole.
void FinePass::drawCracks(spyro::guest_terrain::CrackEdges edges,
                          const spyro::guest_terrain::CrackPolygon &polygon) {
  std::uint32_t entry = edges.table;
  std::uint32_t remaining = edges.edges;
  while (remaining != 0) {
    const std::uint32_t edge = remaining & 0xFFu;
    remaining >>= 8;
    entry += kCracksPerEdge * kCrackEntrySize;
    if (edge == 0) {
      continue;
    }
    const std::uint32_t first = entry - kCracksPerEdge * kCrackEntrySize;
    emitCrackTriangle(frame_, first, polygon);
    emitCrackTriangle(frame_, first + kCrackEntrySize, polygon);
    if ((edge & kNeighbourDrawnWhole) != 0) {
      emitCrackTriangle(frame_, first + 2 * kCrackEntrySize, polygon);
    }
  }
}

// 8002862C.
void FinePass::quad(const SplitSector &sector, const SplitRecord &record) {
  const PackedIndices vertices(record.indices);
  const std::array<GridPoint, 4> corners = {unpackGridVertex(frame_, sector, vertices.first),
                                            unpackGridVertex(frame_, sector, vertices.second),
                                            unpackGridVertex(frame_, sector, vertices.fourth),
                                            unpackGridVertex(frame_, sector, vertices.third)};
  grid::storeFineQuad(frame_, corners, quadColours(sector, record));
  projectGrid(frame_, grid::kFineQuadCells, true);
  grid::refineFineInnerPoints(frame_, facts_.fineInnerPoints);
  drawCracks({.edges = quadEdges(record, kNeighbourCoarser), .table = facts_.fineQuadCracks},
             crack(record, kGt4ToGt3));
  drawQuadPieces(record);
}

// 800294A0: the queue slot is written before the zero-sum test, and kept only past it.
bool FinePass::queue(GpuSpan span, std::uint32_t sum) {
  if (span == GpuSpan::Within) {
    return sum != 0;
  }
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const std::uint32_t cursor = memory.r32(kSplitPrimitiveCursor);
  memory.w32(cursor, frame_.primitive);
  frame_.v1 = cursor;
  if (sum == 0) {
    return false;
  }
  memory.w32(kSplitPrimitiveCursor, cursor + 4);
  return true;
}

// 80029204.
void FinePass::drawQuadPieces(const SplitRecord &record) {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kPieces; ++k) {
    const std::uint32_t first = memory.r32(facts_.fineQuadPieces + 4 * k);
    frame_.v1 = first;
    const std::array<std::uint32_t, 4> cells = {
        first, first + kCellSize, first + kRowStride, first + kRowStride + kCellSize};
    std::uint32_t shared = 0xFFFFFFFFu;
    std::uint32_t sum = 0;
    for (const std::uint32_t at : cells) {
      const std::uint32_t code = memory.r32(at + cell::kDepth);
      shared &= code;
      sum += code & kDepthMask;
    }
    if ((shared & kOutcodeMask) != 0) {
      continue;
    }
    const std::uint32_t prim = frame_.primitive;
    memory.w32(prim + packet::kColour0, memory.r32(cells[0] + cell::kColour));
    memory.w32(prim + packet::kColour1, memory.r32(cells[1] + cell::kColour));
    memory.w32(prim + packet::kColour2, memory.r32(cells[2] + cell::kColour));
    memory.w32(prim + packet::kColour3, memory.r32(cells[3] + cell::kColour));
    writePieceUvs({.pair = record.texture + kFinePairs + 8 * k, .span = kPieceUvSpan});
    std::array<std::uint32_t, 4> screens{};
    for (std::size_t i = 0; i < cells.size(); ++i) {
      screens[i] = memory.r32(cells[i] + cell::kScreen);
    }
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
    const GpuSpan span = gpuSpan(screens);
    if (span == GpuSpan::Within) {
      frame_.v1 = sum >> 7;
    }
    if (!queue(span, sum)) {
      continue;
    }
    memory.w32(prim, spyro::guest_terrain::kQuadTag);
    frame_.v1 = frame_
                    .linkAndAdvance((sum >> 7) + binBias(record.draw),
                                    PacketSize{spyro::guest_terrain::kQuadBytes})
                    .head;
  }
  frame_.v0 = facts_.fineQuadPieces + 4 * kPieces;
}

// 80029588.
void FinePass::triangle(const SplitSector &sector, const SplitRecord &record) {
  const PackedIndices vertices(record.indices);
  const std::array<GridPoint, 3> corners = {unpackGridVertex(frame_, sector, vertices.first),
                                            unpackGridVertex(frame_, sector, vertices.second),
                                            unpackGridVertex(frame_, sector, vertices.third)};
  grid::storeFineTriangle(frame_, corners, triangleColours(sector, record));
  projectGrid(frame_, grid::kFineTriangleCells, true);
  const std::uint32_t corner = (record.links >> kCornerShift) & 3u;
  drawCracks({.edges = triangleEdges(record, kNeighbourCoarser),
              .table = facts_.fineTriangleCracks + corner * kTriangleCrackCorner},
             crack(record, 0));
  drawTrianglePieces(record);
}

// 80029E18.
void FinePass::drawTrianglePieces(const SplitRecord &record) {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const auto half = [](std::uint32_t sxy, GpuSpan span) {
    const std::uint32_t bits = span == GpuSpan::TooHigh ? sxy >> 16 : sxy & 0xFFFFu;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(bits)));
  };
  for (std::uint32_t k = 0; k < kPieces; ++k) {
    const std::uint32_t layout = memory.r32(facts_.fineTrianglePieces + 4 * k);
    writeTrianglePieceUvs(
        record,
        layout,
        {.subTextures = facts_.fineTriangleSubTexture, .selectMask = 0x3C, .pairs = kFinePairs});
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
    const std::uint32_t prim = frame_.primitive;
    memory.w32(prim + packet::kColour0, memory.r32(cells[0] + cell::kColour));
    memory.w32(prim + packet::kColour1, memory.r32(cells[1] + cell::kColour));
    memory.w32(prim + packet::kColour2, memory.r32(cells[2] + cell::kColour));
    const std::uint32_t sum = (d0 & kDepthMask) + (d1 & kDepthMask) + 2 * (d2 & kDepthMask);
    const std::array<std::uint32_t, 3> screens = {memory.r32(cells[0] + cell::kScreen),
                                                  memory.r32(cells[1] + cell::kScreen),
                                                  memory.r32(cells[2] + cell::kScreen)};
    frame_.v0 = screens[1];
    frame_.v1 = screens[2];
    if (record.overflow && !triangleFacesCamera(frame_, screens, sidedness(record.draw))) {
      memory.w32(prim + packet::kXy0, screens[0]);
      continue;
    }
    memory.w32(prim + packet::kXy0, screens[0]);
    memory.w32(prim + packet::kXy1, screens[1]);
    memory.w32(prim + packet::kXy2, screens[2]);
    const GpuSpan span = gpuSpan(screens);
    // The span test leaves the halves it compared last in v0 and v1.
    frame_.v0 = half(screens[1], span);
    frame_.v1 = span == GpuSpan::Within ? sum >> 7 : half(screens[2], span);
    if (!queue(span, sum)) {
      continue;
    }
    memory.w32(prim, spyro::guest_terrain::kTriangleTag);
    frame_.v0 = frame_
                    .linkAndAdvance((sum >> 7) + binBias(record.draw),
                                    PacketSize{spyro::guest_terrain::kTriangleBytes})
                    .head;
    frame_.v1 = prim >> 16;
  }
}

} // namespace

void splitFinePolygons(TerrainFrame &frame, const Facts &facts) {
  FinePass quads(frame, facts, false);
  walkSplitList(frame, frame.scratch + kFineSplitList, quads);
  FinePass triangles(frame, facts, true);
  walkSplitList(frame, frame.scratch + kFineSplitList, triangles);
}

} // namespace spyro1::terrain
