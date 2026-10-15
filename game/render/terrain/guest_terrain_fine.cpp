#include "gte_registers.h"
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
#include "guest_terrain_frame.h"
#include "guest_terrain_grid.h"
#include "guest_terrain_passes.h"
#include "guest_terrain_polygon.h"
#include "guest_terrain_split.h"
#include "guest_word.h"

#include <array>
#include <cstddef>

namespace spyro::guest_terrain {
namespace {

// The GTE register and command numbers are shared vocabulary (psxport gte_registers.h), not this
// pass's.
namespace gte = psx::gte;

using spyro::guest_word::asSigned;

// ── This image's tables are on `frame_.facts.fine` ───────────────────────────────────────────────
// Each is named at its use below with the instruction that reaches it in SCUS_944.25. Only
// `pageRemaps` is the same table the coarse pass uses; every other name here is a DIFFERENT table
// from the coarse pass's table of the same name, which is why they are not shared.

constexpr std::uint32_t kCrackEntrySize = 12;
constexpr std::uint32_t kCracksPerEdge = 2;

constexpr std::uint32_t kGt4ToGt3 = 0x08000000u;
constexpr std::uint32_t kNeighbourDeferredCoarse = 2;
constexpr std::uint32_t kUvAverageMask = 0xFEFFu;
constexpr std::uint32_t kRowStride = 5 * kCellSize;

struct PieceKind {
  std::uint32_t tag = 0;
  PacketSize size;
};
constexpr PieceKind kQuadPiece{.tag = kQuadTag, .size = PacketSize{kQuadBytes}};
constexpr PieceKind kTrianglePiece{.tag = kTriangleTag, .size = PacketSize{kTriangleBytes}};

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
  void drawCracks(CrackEdges edges, const CrackPolygon &polygon);
  void drawQuadPieces(const SplitPolygon &polygon, std::uint32_t draw);
  void drawTrianglePieces(const SplitPolygon &polygon, std::uint32_t draw);
  Linked linkPiece(std::uint32_t bin, const PieceKind &kind, bool resplit);

  TerrainFrame &frame_;
  Core &core_;
  guest_memory::WorkingMemory &memory_;
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
  const PackedIndices vertices(memory_.r32(polygon.record + polygon::kIndices));
  const GridPoint a = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint b = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint c = unpackGridVertex(frame_, sector, vertices.fourth);
  const GridPoint d = unpackGridVertex(frame_, sector, vertices.third);
  const std::uint32_t links = memory_.r32(polygon.record + polygon::kLinks);
  const std::uint32_t draw = memory_.r32(polygon.record + polygon::kDraw);

  // 8002713C.
  const PackedIndices colourIndices(memory_.r32(polygon.record + polygon::kColours));
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t page = memory_.r32(texture + 4);
  const std::uint32_t code = asSigned(page) > 0 ? kOpaqueQuadCode : kSemiQuadCode;
  std::array<std::uint32_t, 4> colours = {memory_.r32(sector.colours + colourIndices.first) + code,
                                          memory_.r32(sector.colours + colourIndices.second) + code,
                                          memory_.r32(sector.colours + colourIndices.fourth) + code,
                                          memory_.r32(sector.colours + colourIndices.third) + code};
  loadCornerColours(frame_, colours.data(), 4);
  grid::storeFineQuad(frame_, {a, b, c, d}, colours);

  projectGrid(frame_, grid::kFineQuadCells, true);
  grid::refineFineInnerPoints(frame_, frame_.facts.fine.innerPoints);
  const std::uint32_t uv = memory_.r32(texture);
  drawCracks({.edges = quadNeighbourEdges(frame_, links, draw, kNeighbourDeferredCoarse),
              .table = frame_.facts.fine.quadCracks},
             {.colourAdjust = kGt4ToGt3, .uv = uv, .page = page, .binBias = binBias(draw)});
  drawQuadPieces(polygon, draw);
}

// 8002763C: two crack triangles per edge whose neighbour byte is set.
void FineSplitPass::drawCracks(CrackEdges edges, const CrackPolygon &polygon) {
  std::uint32_t entry = edges.table;
  std::uint32_t remaining = edges.edges;
  while (remaining != 0) {
    const std::uint32_t edge = remaining & 0xFFu;
    remaining >>= 8;
    entry += kCracksPerEdge * kCrackEntrySize;
    if (edge == 0) {
      continue;
    }
    for (std::uint32_t i = kCracksPerEdge; i > 0; --i) {
      const std::uint32_t crack = entry - i * kCrackEntrySize;
      emitCrackTriangle(frame_, crack, polygon);
    }
  }
}

// Link a piece, first recording it for the re-split pass when it is too large for the GPU.
Linked FineSplitPass::linkPiece(std::uint32_t bin, const PieceKind &kind, bool resplit) {
  const std::uint32_t prim = frame_.primitive;
  if (resplit) {
    const std::uint32_t cursor = memory_.r32(kSplitPrimitiveCursor);
    memory_.w32(cursor, prim);
    memory_.w32(kSplitPrimitiveCursor, cursor + 4);
  }
  memory_.w32(prim, kind.tag);
  return frame_.linkAndAdvance(bin, kind.size);
}

// 8002775C: the sixteen GT4 pieces. Each quadrant of the quad has its own texture pair, and each
// piece takes the quarter of it between the pair's corner nearest the piece and the other three.
void FineSplitPass::drawQuadPieces(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t quadrants = texture + 0x10;
  for (std::uint32_t k = 0; k < 16; ++k) {
    const std::uint32_t word = memory_.r32(frame_.facts.fine.quadPieces + 4 * k);
    const std::uint32_t quadrant = word & 0xFu;
    const std::uint32_t first = word ^ quadrant;
    const std::array<std::uint32_t, 4> pieceCells = {
        first, first + kCellSize, first + kRowStride, first + kRowStride + kCellSize};
    std::array<std::uint32_t, 4> codes{};
    for (std::size_t i = 0; i < 4; ++i) {
      codes[i] = memory_.r32(pieceCells[i] + cell::kDepth);
    }
    const std::uint32_t pair = quadrants + ((quadrant & 0xCu) << 1);
    std::uint32_t uv = memory_.r32(pair);
    if ((codes[0] & codes[1] & codes[2] & codes[3] & kOutcodeMask) != 0) {
      continue;
    }
    const std::uint32_t page = memory_.r32(pair + 4);
    const std::uint32_t sum = (codes[0] & kDepthMask) + (codes[1] & kDepthMask) +
                              (codes[2] & kDepthMask) + (codes[3] & kDepthMask);
    if (sum == 0) {
      continue;
    }
    const std::uint32_t prim = frame_.primitive;
    memory_.w32(prim + packet::kColour0, memory_.r32(pieceCells[0] + cell::kColour));
    memory_.w32(prim + packet::kColour1, memory_.r32(pieceCells[1] + cell::kColour));
    memory_.w32(prim + packet::kColour2, memory_.r32(pieceCells[2] + cell::kColour));
    memory_.w32(prim + packet::kColour3, memory_.r32(pieceCells[3] + cell::kColour));

    // 800277F8: the CLUT and page halfwords, then the four corner UVs of the pair (remapped like
    // the coarse pass's) and the piece's UVs halfway from its own corner to each of them.
    memory_.w16(prim + packet::kUv0 + 2, static_cast<std::uint16_t>(uv >> 16));
    uv &= 0xFFFFu;
    memory_.w16(prim + packet::kUv1 + 2, static_cast<std::uint16_t>(page >> 16));
    std::array<std::uint32_t, 4> corners = {uv, uv + 0x1Fu, uv + 0x1F00u, page & 0xFFFFu};
    const std::uint32_t remap = (page >> 25) & 0x38u;
    if (remap != 0) {
      const std::uint32_t rest = memory_.r32(frame_.facts.fine.pageRemaps + remap + 4);
      const std::uint32_t head = memory_.r32(frame_.facts.fine.pageRemaps + remap);
      corners[2] = uv + (rest & 0xFFFFu);
      corners[3] = uv + (rest >> 16);
      corners[0] = uv + (head & 0xFFFFu);
      corners[1] += static_cast<std::uint32_t>(asSigned(head) >> 16);
    }
    const std::uint32_t own = corners[quadrant & 3u] & kUvAverageMask;
    const std::array<std::uint32_t, 4> slots = {
        packet::kUv0, packet::kUv1, packet::kUv2, packet::kUv3};
    for (std::size_t i = 0; i < 4; ++i) {
      memory_.w16(prim + slots[i],
                  static_cast<std::uint16_t>(((corners[i] & kUvAverageMask) + own) >> 1));
    }

    std::array<std::uint32_t, 4> screens{};
    for (std::size_t i = 0; i < 4; ++i) {
      screens[i] = memory_.r32(pieceCells[i] + cell::kScreen);
    }
    if (polygon.overflow && !facesCamera(frame_,
                                         {screens[0], screens[1], screens[2], screens[3]},
                                         sidedness(draw),
                                         SecondTriangle::FourthFirst)) {
      memory_.w32(prim + packet::kXy0, screens[0]);
      continue;
    }
    memory_.w32(prim + packet::kXy0, screens[0]);
    memory_.w32(prim + packet::kXy1, screens[1]);
    memory_.w32(prim + packet::kXy2, screens[2]);
    memory_.w32(prim + packet::kXy3, screens[3]);
    frame_.v1 =
        linkPiece(polygonBin(sum, draw), kQuadPiece, gpuSpan(screens) != GpuSpan::Within).head;
  }
  // The loop's exit test reads one word past the table into v1.
  frame_.v0 = frame_.facts.fine.quadPieces + 16 * 4;
  frame_.v1 = memory_.r32(frame_.facts.fine.quadPieces + 16 * 4);
}

// 80027B8C.
void FineSplitPass::triangle(const SplitSector &sector, const SplitPolygon &polygon) {
  const PackedIndices vertices(memory_.r32(polygon.record + polygon::kIndices));
  const GridPoint a = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint b = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint c = unpackGridVertex(frame_, sector, vertices.third);
  const std::uint32_t links = memory_.r32(polygon.record + polygon::kLinks);
  const std::uint32_t draw = memory_.r32(polygon.record + polygon::kDraw);

  const std::uint32_t colourWord = memory_.r32(polygon.record + polygon::kColours);
  const PackedIndices colourIndices(colourWord);
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t page = memory_.r32(texture + 4);
  const std::uint32_t code = asSigned(page) > 0 ? kOpaqueTriangleCode : kSemiTriangleCode;
  std::array<std::uint32_t, 3> colours = {memory_.r32(sector.colours + colourIndices.first) + code,
                                          memory_.r32(sector.colours + colourIndices.second) + code,
                                          memory_.r32(sector.colours + colourIndices.third) + code};
  loadCornerColours(frame_, colours.data(), 3);
  grid::storeFineTriangle(frame_, {a, b, c}, colours);

  projectGrid(frame_, grid::kFineTriangleCells, true);
  const std::uint32_t uv = memory_.r32(texture);
  const std::uint32_t corner = draw & kTriangleCornerMask;
  drawCracks({.edges = triangleNeighbourEdges(frame_, links, colourWord, kNeighbourDeferredCoarse),
              .table = frame_.facts.fine.triangleCracks + (corner >> 6) + (corner >> 9)},
             {.colourAdjust = 0, .uv = uv, .page = page, .binBias = binBias(draw)});
  drawTrianglePieces(polygon, draw);
}

// 80028248: the sixteen GT3 pieces, each a sub-texture of the record chosen by the dropped
// corner, shifted by the image's piece-shift table.
void FineSplitPass::drawTrianglePieces(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t corner = (draw >> 12) & 3u;
  const std::uint32_t subTextures = texture + 0x10;
  for (std::uint32_t k = 0; k < 16; ++k) {
    const std::uint32_t layout = memory_.r32(frame_.facts.fine.trianglePieces + 4 * k);
    const auto select =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(
            memory_.r8(frame_.facts.fine.pieceTexture + corner + (layout & 0x3Cu)))));
    const std::array<std::uint32_t, 3> pieceCells = {kScratchpad + ((layout >> 20) & 0xFF0u),
                                                     kScratchpad + ((layout >> 12) & 0xFF0u),
                                                     kScratchpad + ((layout >> 4) & 0xFF0u)};
    const std::uint32_t pair = (select & 0x18u) + subTextures;
    std::uint32_t page = memory_.r32(pair + 4);
    const std::uint32_t shift =
        memory_.r32(frame_.facts.fine.pieceShift + ((select & 6u) << 1) + ((page >> 24) & 0x70u));
    std::uint32_t uv = memory_.r32(pair);
    page += static_cast<std::uint32_t>(asSigned(shift) >> 16);
    uv += shift & 0xFFFFu;
    const std::uint32_t prim = frame_.primitive;
    writePieceTriangleUvs(memory_,
                          prim,
                          {.uv = uv, .page = page},
                          {.table = frame_.facts.fine.triangleUvs, .turn = corner + layout},
                          kEnginePieceUvs);

    std::array<std::uint32_t, 3> codes{};
    for (std::size_t i = 0; i < 3; ++i) {
      codes[i] = memory_.r32(pieceCells[i] + cell::kDepth);
    }
    frame_.v0 = codes[1];
    frame_.v1 = codes[2];
    if ((codes[0] & codes[1] & codes[2] & kOutcodeMask) != 0) {
      continue;
    }
    memory_.w32(prim + packet::kColour0, memory_.r32(pieceCells[0] + cell::kColour));
    memory_.w32(prim + packet::kColour1, memory_.r32(pieceCells[1] + cell::kColour));
    memory_.w32(prim + packet::kColour2, memory_.r32(pieceCells[2] + cell::kColour));
    const std::uint32_t sum =
        (codes[0] & kDepthMask) + (codes[1] & kDepthMask) + 2 * (codes[2] & kDepthMask);
    frame_.v0 = codes[1] & kDepthMask;
    frame_.v1 = codes[2] & kDepthMask;
    if (sum == 0) {
      continue;
    }
    std::array<std::uint32_t, 3> screens{};
    for (std::size_t i = 0; i < 3; ++i) {
      screens[i] = memory_.r32(pieceCells[i] + cell::kScreen);
    }
    frame_.v0 = screens[1];
    frame_.v1 = screens[2];
    if (polygon.overflow &&
        !triangleFacesCamera(frame_, {screens[0], screens[1], screens[2]}, sidedness(draw))) {
      memory_.w32(prim + packet::kXy0, screens[0]);
      continue;
    }
    memory_.w32(prim + packet::kXy0, screens[0]);
    memory_.w32(prim + packet::kXy1, screens[1]);
    memory_.w32(prim + packet::kXy2, screens[2]);
    const Linked linked =
        linkPiece(polygonBin(sum, draw), kTrianglePiece, gpuSpan(screens) != GpuSpan::Within);
    frame_.v0 = linked.head;
    frame_.v1 = linked.primitive >> 16;
  }
}

} // namespace

void splitFinePolygons(TerrainFrame &frame) {
  frame.memory.w32(kSplitPrimitiveCursor, frame.scratch + frame.facts.resplit.queue);
  FineSplitPass quads(frame, false);
  walkSplitList(frame, frame.scratch + kFineSplitList, quads);
  FineSplitPass triangles(frame, true);
  walkSplitList(frame, frame.scratch + kFineSplitList, triangles);
}

} // namespace spyro::guest_terrain
