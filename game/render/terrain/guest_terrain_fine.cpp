// guest_terrain_fine.cpp — the fine subdivision passes of the terrain drawer of this engine family
// (SCUS_944.25 80026C74..80028504): polygons the near passes deferred because a vertex came nearer
// than 0x140, each cut 4x4.
//
// The quad pass (80026C74) walks kFineSplitList skipping triangles; the triangle pass (80027AC8)
// walks it again skipping quads. Each polygon becomes a 5x5 (quad) or 15-point triangular grid
// (guest_terrain_split.h), projected with points at zero depth marked off every edge. A quad's
// four inner diagonal points are pulled toward their neighbours when every corner is past 0x40.
// Edges whose neighbour was deferred coarse get two crack triangles each, and every sub-polygon
// that is too large for the GPU (0x200 high or 0x400 wide) is still drawn but also recorded in
// the re-split list at kSplitPrimitiveCursor for the pass after this one.

#include "core.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/guest_terrain_grid.h"
#include "render/terrain/guest_terrain_passes.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "render/terrain/guest_terrain_split.h"

#include <array>
#include <cstddef>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kNeighbourDeferredCoarse = 2;
// 8002763C: two crack triangles per edge whose neighbour byte is set.
constexpr CrackRun kCracks{.perEdge = 2};
constexpr std::uint32_t kUvAverageMask = 0xFEFFu;
constexpr std::uint32_t kRowStride = 5 * kCellSize;

class FineSplitPass final : public SplitPolygonPass {
public:
  FineSplitPass(TerrainFrame &frame, bool triangles)
      : frame_(frame), core_(frame.core), memory_(frame.memory), triangles_(triangles) {}

  SplitSector sector(std::uint32_t header) override {
    return readSplitHeader(frame_, header);
  }
  void polygon(const SplitSector &sector, const SplitPolygon &polygon) override;

private:
  void quad(const SplitSector &sector, const SplitPolygon &polygon);
  void triangle(const SplitSector &sector, const SplitPolygon &polygon);
  void drawQuadPieces(const SplitPolygon &polygon, std::uint32_t draw);
  void drawTrianglePieces(const SplitPolygon &polygon, std::uint32_t draw);
  void queueTooLarge(GpuSpan span);

  TerrainFrame &frame_;
  Core &core_;
  const psx::present::EmitMemory &memory_;
  bool triangles_;
};

void FineSplitPass::polygon(const SplitSector &sector, const SplitPolygon &polygon) {
  if (polygon.triangle != triangles_) {
    return;
  }
  if (polygon.triangle) {
    triangle(sector, polygon);
  } else {
    quad(sector, polygon);
  }
}

// 80026D40.
void FineSplitPass::quad(const SplitSector &sector, const SplitPolygon &polygon) {
  const PackedIndices vertices(memory_.mem_r32(polygon.record + polygon::kIndices));
  const GridPoint a = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint b = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint c = unpackGridVertex(frame_, sector, vertices.fourth);
  const GridPoint d = unpackGridVertex(frame_, sector, vertices.third);
  const std::uint32_t links = memory_.mem_r32(polygon.record + polygon::kLinks);
  const std::uint32_t draw = memory_.mem_r32(polygon.record + polygon::kDraw);

  // 8002713C.
  const PackedIndices colourIndices(memory_.mem_r32(polygon.record + polygon::kColours));
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t page = memory_.mem_r32(texture + 4);
  const std::uint32_t code = asSigned(page) > 0 ? guest_packet::kGt4Code : guest_packet::kSemiGt4Code;
  std::array<std::uint32_t, 4> colours = {memory_.mem_r32(sector.colours + colourIndices.first) + code,
                                          memory_.mem_r32(sector.colours + colourIndices.second) + code,
                                          memory_.mem_r32(sector.colours + colourIndices.fourth) + code,
                                          memory_.mem_r32(sector.colours + colourIndices.third) + code};
  loadCornerColours(frame_, colours.data(), 4);
  grid::storeFineQuad(frame_, {a, b, c, d}, colours);

  projectGrid(frame_, grid::kFineQuadCells, true);
  grid::refineFineInnerPoints(frame_, frame_.facts.fine.innerPoints);
  const std::uint32_t uv = memory_.mem_r32(texture);
  drawEdgeCracks(frame_,
                 {.edges = quadNeighbourEdges(frame_, links, draw, kNeighbourDeferredCoarse),
                  .table = frame_.facts.fine.quadCracks},
                 {.colourAdjust = kGt4ToGt3, .uv = uv, .page = page, .binBias = binBias(draw)},
                 kCracks);
  drawQuadPieces(polygon, draw);
}

// The piece recorded for the re-split pass when it is too large for the GPU.
void FineSplitPass::queueTooLarge(GpuSpan span) {
  if (span == GpuSpan::Within) {
    return;
  }
  const std::uint32_t cursor = memory_.mem_r32(kSplitPrimitiveCursor);
  memory_.mem_w32(cursor, frame_.primitive);
  memory_.mem_w32(kSplitPrimitiveCursor, cursor + 4);
}

// 8002775C: the sixteen GT4 pieces. Each quadrant of the quad has its own texture pair, and each
// piece takes the quarter of it between the pair's corner nearest the piece and the other three.
void FineSplitPass::drawQuadPieces(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t quadrants = texture + 0x10;
  for (std::uint32_t k = 0; k < 16; ++k) {
    const std::uint32_t word = memory_.mem_r32(frame_.facts.fine.quadPieces + 4 * k);
    const std::uint32_t quadrant = word & 0xFu;
    const std::uint32_t first = word ^ quadrant;
    QuadPiece piece = readPieceDepths<4>(
        memory_, {first, first + kCellSize, first + kRowStride, first + kRowStride + kCellSize});
    const std::uint32_t pair = quadrants + ((quadrant & 0xCu) << 1);
    std::uint32_t uv = memory_.mem_r32(pair);
    if (piece.outside()) {
      continue;
    }
    const std::uint32_t page = memory_.mem_r32(pair + 4);
    const std::uint32_t sum = piece.depthSum();
    if (sum == 0) {
      continue;
    }
    const std::uint32_t prim = frame_.primitive;
    storePieceColours(frame_, piece.cells);

    // 800277F8: the CLUT and page halfwords, then the four corner UVs of the pair (remapped like
    // the coarse pass's) and the piece's UVs halfway from its own corner to each of them.
    memory_.mem_w16(prim + packet::kUv0 + 2, static_cast<std::uint16_t>(uv >> 16));
    uv &= 0xFFFFu;
    memory_.mem_w16(prim + packet::kUv1 + 2, static_cast<std::uint16_t>(page >> 16));
    std::array<std::uint32_t, 4> corners = {uv, uv + 0x1Fu, uv + 0x1F00u, page & 0xFFFFu};
    const std::uint32_t remap = (page >> 25) & 0x38u;
    if (remap != 0) {
      const std::uint32_t rest = memory_.mem_r32(frame_.facts.fine.pageRemaps + remap + 4);
      const std::uint32_t head = memory_.mem_r32(frame_.facts.fine.pageRemaps + remap);
      corners[2] = uv + (rest & 0xFFFFu);
      corners[3] = uv + (rest >> 16);
      corners[0] = uv + (head & 0xFFFFu);
      corners[1] += static_cast<std::uint32_t>(asSigned(head) >> 16);
    }
    const std::uint32_t own = corners[quadrant & 3u] & kUvAverageMask;
    const std::array<std::uint32_t, 4> slots = {
        packet::kUv0, packet::kUv1, packet::kUv2, packet::kUv3};
    for (std::size_t i = 0; i < 4; ++i) {
      memory_.mem_w16(prim + slots[i],
                  static_cast<std::uint16_t>(((corners[i] & kUvAverageMask) + own) >> 1));
    }

    readPieceScreens(memory_, piece);
    if (!pieceFacesCamera(frame_, piece, polygon.overflow, sidedness(draw), SecondTriangle::FourthFirst)) {
      continue;
    }
    storePieceScreens(frame_, piece.screens);
    queueTooLarge(gpuSpan(piece.screens));
    frame_.v1 = linkQuadPiece(frame_, polygonBin(sum, binBias(draw))).head;
  }
  // The loop's exit test reads one word past the table into v1.
  frame_.v0 = frame_.facts.fine.quadPieces + 16 * 4;
  frame_.v1 = memory_.mem_r32(frame_.facts.fine.quadPieces + 16 * 4);
}

// 80027B8C.
void FineSplitPass::triangle(const SplitSector &sector, const SplitPolygon &polygon) {
  const PackedIndices vertices(memory_.mem_r32(polygon.record + polygon::kIndices));
  const GridPoint a = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint b = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint c = unpackGridVertex(frame_, sector, vertices.third);
  const std::uint32_t links = memory_.mem_r32(polygon.record + polygon::kLinks);
  const std::uint32_t draw = memory_.mem_r32(polygon.record + polygon::kDraw);

  const std::uint32_t colourWord = memory_.mem_r32(polygon.record + polygon::kColours);
  const PackedIndices colourIndices(colourWord);
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t page = memory_.mem_r32(texture + 4);
  const std::uint32_t code = asSigned(page) > 0 ? guest_packet::kGt3Code : guest_packet::kSemiGt3Code;
  std::array<std::uint32_t, 3> colours = {memory_.mem_r32(sector.colours + colourIndices.first) + code,
                                          memory_.mem_r32(sector.colours + colourIndices.second) + code,
                                          memory_.mem_r32(sector.colours + colourIndices.third) + code};
  loadCornerColours(frame_, colours.data(), 3);
  grid::storeFineTriangle(frame_, {a, b, c}, colours);

  projectGrid(frame_, grid::kFineTriangleCells, true);
  const std::uint32_t uv = memory_.mem_r32(texture);
  const std::uint32_t corner = draw & kTriangleCornerMask;
  drawEdgeCracks(frame_,
                 {.edges = triangleNeighbourEdges(frame_, links, colourWord, kNeighbourDeferredCoarse),
                  .table = frame_.facts.fine.triangleCracks + (corner >> 6) + (corner >> 9)},
                 {.colourAdjust = 0, .uv = uv, .page = page, .binBias = binBias(draw)},
                 kCracks);
  drawTrianglePieces(polygon, draw);
}

// 80028248: the sixteen GT3 pieces, each a sub-texture of the record chosen by the dropped
// corner, shifted by the image's piece-shift table.
void FineSplitPass::drawTrianglePieces(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t corner = (draw >> 12) & 3u;
  const std::uint32_t subTextures = texture + 0x10;
  for (std::uint32_t k = 0; k < 16; ++k) {
    const std::uint32_t layout = memory_.mem_r32(frame_.facts.fine.trianglePieces + 4 * k);
    const auto select =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(
            memory_.mem_r8(frame_.facts.fine.pieceTexture + corner + (layout & 0x3Cu)))));
    const std::uint32_t pair = (select & 0x18u) + subTextures;
    std::uint32_t page = memory_.mem_r32(pair + 4);
    const std::uint32_t shift =
        memory_.mem_r32(frame_.facts.fine.pieceShift + ((select & 6u) << 1) + ((page >> 24) & 0x70u));
    std::uint32_t uv = memory_.mem_r32(pair);
    page += static_cast<std::uint32_t>(asSigned(shift) >> 16);
    uv += shift & 0xFFFFu;
    const std::uint32_t prim = frame_.primitive;
    writePieceTriangleUvs(memory_,
                          prim,
                          {.uv = uv, .page = page},
                          {.table = frame_.facts.fine.triangleUvs, .turn = corner + layout},
                          kEnginePieceUvs);

    TrianglePiece piece = readPieceDepths<3>(memory_, trianglePieceCells(layout));
    frame_.v0 = piece.depths[1];
    frame_.v1 = piece.depths[2];
    if (piece.outside()) {
      continue;
    }
    storePieceColours(frame_, piece.cells);
    const std::uint32_t sum = piece.depthSum();
    frame_.v0 = piece.depths[1] & kDepthMask;
    frame_.v1 = piece.depths[2] & kDepthMask;
    if (sum == 0) {
      continue;
    }
    readPieceScreens(memory_, piece);
    frame_.v0 = piece.screens[1];
    frame_.v1 = piece.screens[2];
    if (!pieceFacesCamera(frame_, piece, polygon.overflow, sidedness(draw))) {
      continue;
    }
    storePieceScreens(frame_, piece.screens);
    queueTooLarge(gpuSpan(piece.screens));
    const Linked linked = linkTrianglePiece(frame_, polygonBin(sum, binBias(draw)));
    frame_.v0 = linked.head;
    frame_.v1 = linked.primitive >> 16;
  }
}

} // namespace

void splitFinePolygons(TerrainFrame &frame) {
  frame.memory.mem_w32(kSplitPrimitiveCursor, frame.scratch + frame.facts.resplit.queue);
  FineSplitPass quads(frame, false);
  walkSplitList(frame, frame.scratch + kFineSplitList, quads);
  FineSplitPass triangles(frame, true);
  walkSplitList(frame, frame.scratch + kFineSplitList, triangles);
}

} // namespace spyro::guest_terrain
