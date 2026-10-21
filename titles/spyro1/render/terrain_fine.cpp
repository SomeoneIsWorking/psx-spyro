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
using spyro::guest_terrain::drawEdgeCracks;
using spyro::guest_terrain::gpuSpan;
using spyro::guest_terrain::GpuSpan;
using spyro::guest_terrain::GridPoint;
using spyro::guest_terrain::kCellSize;
using spyro::guest_terrain::kGt4ToGt3;
using spyro::guest_terrain::kSplitPrimitiveCursor;
using spyro::guest_terrain::linkQuadPiece;
using spyro::guest_terrain::linkTrianglePiece;
using spyro::guest_terrain::PackedIndices;
using spyro::guest_terrain::pieceFacesCamera;
using spyro::guest_terrain::polygonBin;
using spyro::guest_terrain::QuadPiece;
using spyro::guest_terrain::readPieceDepths;
using spyro::guest_terrain::readPieceScreens;
using spyro::guest_terrain::SecondTriangle;
using spyro::guest_terrain::storePieceColours;
using spyro::guest_terrain::storePieceScreens;
using spyro::guest_terrain::TrianglePiece;
using spyro::guest_terrain::trianglePieceCells;

using spyro::guest_word::signedHalf;

constexpr std::uint32_t kNeighbourCoarser = 3; // drawn whole or deferred coarse
// 80028ED8: two crack triangles per edge whose neighbour is coarser, and a third where it was drawn whole.
constexpr spyro::guest_terrain::CrackRun kCracks{.perEdge = 3, .drawnWholeOnly = 1};
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
  drawEdgeCracks(frame_,
                 {.edges = quadEdges(record, kNeighbourCoarser), .table = facts_.fineQuadCracks},
                 crack(record, kGt4ToGt3),
                 kCracks);
  drawQuadPieces(record);
}

// 800294A0: the queue slot is written before the zero-sum test, and kept only past it.
bool FinePass::queue(GpuSpan span, std::uint32_t sum) {
  if (span == GpuSpan::Within) {
    return sum != 0;
  }
  const psx::present::EmitMemory &memory = frame_.memory;
  const std::uint32_t cursor = memory.mem_r32(kSplitPrimitiveCursor);
  memory.mem_w32(cursor, frame_.primitive);
  frame_.v1 = cursor;
  if (sum == 0) {
    return false;
  }
  memory.mem_w32(kSplitPrimitiveCursor, cursor + 4);
  return true;
}

// 80029204.
void FinePass::drawQuadPieces(const SplitRecord &record) {
  const psx::present::EmitMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kPieces; ++k) {
    const std::uint32_t first = memory.mem_r32(facts_.fineQuadPieces + 4 * k);
    frame_.v1 = first;
    QuadPiece piece =
        readPieceDepths<4>(memory, {first, first + kCellSize, first + kRowStride, first + kRowStride + kCellSize});
    if (piece.outside()) {
      continue;
    }
    const std::uint32_t sum = piece.depthSum();
    storePieceColours(frame_, piece.cells);
    writePieceUvs({.pair = record.texture + kFinePairs + 8 * k, .span = kPieceUvSpan});
    readPieceScreens(memory, piece);
    if (!pieceFacesCamera(frame_, piece, record.overflow, sidedness(record.draw), SecondTriangle::Reversed)) {
      continue;
    }
    storePieceScreens(frame_, piece.screens);
    const GpuSpan span = gpuSpan(piece.screens);
    if (span == GpuSpan::Within) {
      frame_.v1 = sum >> 7;
    }
    if (!queue(span, sum)) {
      continue;
    }
    frame_.v1 = linkQuadPiece(frame_, polygonBin(sum, binBias(record.draw))).head;
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
  drawEdgeCracks(frame_,
                 {.edges = triangleEdges(record, kNeighbourCoarser),
                  .table = facts_.fineTriangleCracks + corner * kTriangleCrackCorner},
                 crack(record, 0),
                 kCracks);
  drawTrianglePieces(record);
}

// 80029E18.
void FinePass::drawTrianglePieces(const SplitRecord &record) {
  const psx::present::EmitMemory &memory = frame_.memory;
  const auto half = [](std::uint32_t sxy, GpuSpan span) {
    return signedHalf(span == GpuSpan::TooHigh ? sxy >> 16 : sxy);
  };
  for (std::uint32_t k = 0; k < kPieces; ++k) {
    const std::uint32_t layout = memory.mem_r32(facts_.fineTrianglePieces + 4 * k);
    writeTrianglePieceUvs(
        record,
        layout,
        {.subTextures = facts_.fineTriangleSubTexture, .selectMask = 0x3C, .pairs = kFinePairs});
    TrianglePiece piece = readPieceDepths<3>(memory, trianglePieceCells(layout));
    frame_.v0 = piece.depths[1];
    frame_.v1 = piece.depths[2];
    if (piece.outside()) {
      continue;
    }
    const std::uint32_t prim = frame_.primitive;
    storePieceColours(frame_, piece.cells);
    const std::uint32_t sum = piece.depthSum();
    readPieceScreens(memory, piece);
    frame_.v0 = piece.screens[1];
    frame_.v1 = piece.screens[2];
    if (!pieceFacesCamera(frame_, piece, record.overflow, sidedness(record.draw))) {
      continue;
    }
    storePieceScreens(frame_, piece.screens);
    const GpuSpan span = gpuSpan(piece.screens);
    // The span test leaves the halves it compared last in v0 and v1.
    frame_.v0 = half(piece.screens[1], span);
    frame_.v1 = span == GpuSpan::Within ? sum >> 7 : half(piece.screens[2], span);
    if (!queue(span, sum)) {
      continue;
    }
    frame_.v0 = linkTrianglePiece(frame_, polygonBin(sum, binBias(record.draw))).head;
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
