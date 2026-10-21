// SCUS_942.28 80027688..80028590: the coarse subdivision pass. Each polygon the detail pass
// deferred coarse is cut 2x2 on the engine family's grid, its edges toward neighbours drawn whole
// closed with one crack triangle each, and its four pieces drawn from the split-texture record's
// own pairs.

#include "core.h"
#include "render/terrain/guest_terrain_grid.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "spyro1/render/terrain_split.h"

#include <array>

namespace spyro1::terrain {
namespace {

namespace grid = spyro::guest_terrain::grid;
using spyro::guest_terrain::GridPoint;
using spyro::guest_terrain::drawEdgeCracks;
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

constexpr std::uint32_t kNeighbourDrawnWhole = 1;
constexpr std::uint32_t kPieceUvSpan = 0x1F;
constexpr std::uint32_t kQuadPieces = 4;
constexpr std::uint32_t kTrianglePieces = 4;

class CoarsePass final : public SplitPass {
public:
  CoarsePass(TerrainFrame &frame, const Facts &facts) : SplitPass(frame, facts) {}

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
  drawEdgeCracks(frame_,
                 {.edges = quadEdges(record, kNeighbourDrawnWhole), .table = facts_.coarseQuadCracks},
                 crack(record, kGt4ToGt3),
                 {});

  // 80027D7C: the four GT4s.
  const psx::present::EmitMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kQuadPieces; ++k) {
    const std::uint32_t first = memory.mem_r32(facts_.coarseQuadCells + 4 * k);
    frame_.v1 = first;
    QuadPiece piece =
        readPieceDepths<4>(memory, {first, first + kCellSize, first + 3 * kCellSize, first + 4 * kCellSize});
    if (piece.outside()) {
      continue;
    }
    readPieceScreens(memory, piece);
    if (!pieceFacesCamera(frame_, piece, record.overflow, sidedness(record.draw), SecondTriangle::Reversed)) {
      continue;
    }
    storePieceScreens(frame_, piece.corners());
    storePieceColours(frame_, piece.cells);
    writePieceUvs({.pair = record.texture + kCoarsePairs + 8 * k, .span = kPieceUvSpan});
    frame_.v1 = linkQuadPiece(frame_, polygonBin(piece.depthSum(), binBias(record.draw))).head;
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
  drawEdgeCracks(frame_,
                 {.edges = triangleEdges(record, kNeighbourDrawnWhole),
                  .table = facts_.coarseTriangleCracks + (corner >> 3) + (corner >> 6)},
                 crack(record, 0),
                 {});

  // 800283DC: the four GT3s.
  const psx::present::EmitMemory &memory = frame_.memory;
  for (std::uint32_t k = 0; k < kTrianglePieces; ++k) {
    const std::uint32_t layout = memory.mem_r32(facts_.coarseTriangleCells + 4 * k);
    writeTrianglePieceUvs(
        record,
        layout,
        {.subTextures = facts_.coarseTriangleSubTexture, .selectMask = 0xC, .pairs = kCoarsePairs});
    TrianglePiece piece = readPieceDepths<3>(memory, trianglePieceCells(layout));
    frame_.v0 = piece.depths[1];
    frame_.v1 = piece.depths[2];
    if (piece.outside()) {
      continue;
    }
    readPieceScreens(memory, piece);
    frame_.v0 = piece.screens[1];
    frame_.v1 = piece.screens[2];
    const std::uint32_t prim = frame_.primitive;
    if (!pieceFacesCamera(frame_, piece, record.overflow, sidedness(record.draw))) {
      continue;
    }
    storePieceScreens(frame_, piece.corners());
    storePieceColours(frame_, piece.cells);
    frame_.v0 = linkTrianglePiece(frame_, polygonBin(piece.depthSum(), binBias(record.draw))).head;
    frame_.v1 = prim >> 16;
  }
}

} // namespace

void splitCoarsePolygons(TerrainFrame &frame, const Facts &facts) {
  CoarsePass pass(frame, facts);
  walkSplitList(frame, frame.scratch + kCoarseSplitList, pass);
  // 8002857C: the re-split queue starts where the coarse list did.
  frame.memory.mem_w32(kSplitPrimitiveCursor, frame.scratch + frame.facts.resplit.queue);
}

} // namespace spyro1::terrain
