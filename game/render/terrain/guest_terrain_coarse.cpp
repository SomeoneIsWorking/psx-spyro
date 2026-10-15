#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_fog.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/guest_terrain_grid.h"
#include "render/terrain/guest_terrain_passes.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "render/terrain/guest_terrain_split.h"

#include <array>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kGt4ToGt3 = 0x08000000u; // 0x3C/0x3E -> 0x34/0x36
constexpr std::uint32_t kTexelSpan = 0x1F00;

// 8002654C: page bits 28..30 select a remap that rotates or mirrors the texture by moving its
// corners; zero is the identity.
void writeQuadUvs(guest_memory::WorkingMemory &memory,
                  std::uint32_t prim,
                  TextureWords texture,
                  std::uint32_t pageRemaps) {
  const std::uint32_t uv = texture.uv;
  const std::uint32_t page = texture.page;
  std::uint32_t first = uv;
  std::uint32_t second = page - kTexelSpan;
  std::uint32_t third = uv + kTexelSpan;
  std::uint32_t fourth = page;
  const std::uint32_t remap = (second >> 25) & 0x38u;
  if (remap != 0) {
    const std::uint32_t rest = memory.r32(pageRemaps + remap + 4);
    const std::uint32_t head = memory.r32(pageRemaps + remap);
    third = uv + (rest & 0xFFFFu);
    fourth = uv + (rest >> 16);
    first = uv + (head & 0xFFFFu);
    second += static_cast<std::uint32_t>(asSigned(head) >> 16);
  }
  memory.w32(prim + packet::kUv0, first);
  memory.w32(prim + packet::kUv1, second);
  memory.w32(prim + packet::kUv2, third);
  memory.w32(prim + packet::kUv3, fourth);
}

class CoarseSplitPass final : public SplitPolygonPass {
public:
  explicit CoarseSplitPass(TerrainFrame &frame)
      : frame_(frame), core_(frame.core), memory_(frame.memory) {}

  void run();
  SplitSector sector(std::uint32_t header) override {
    return readSplitHeader(frame_, header);
  }
  void polygon(const SplitSector &sector, const SplitPolygon &polygon) override;

private:
  void quad(const SplitSector &sector, const SplitPolygon &polygon);
  void triangle(const SplitSector &sector, const SplitPolygon &polygon);
  void drawSubQuads(const SplitPolygon &polygon, std::uint32_t draw);
  void drawSubTriangles(const SplitPolygon &polygon, std::uint32_t draw);

  TerrainFrame &frame_;
  Core &core_;
  guest_memory::WorkingMemory &memory_;
};

void CoarseSplitPass::run() {
  loadFogColour(memory_, core_, frame_.facts); // 80025CD0
  walkSplitList(frame_, frame_.scratch + kCoarseSplitList, *this);
}

void CoarseSplitPass::polygon(const SplitSector &sector, const SplitPolygon &polygon) {
  if (polygon.triangle) {
    triangle(sector, polygon);
  } else {
    quad(sector, polygon);
  }
}

// 80025DC4: a quad cut 2x2. Its corners are cells 0, 2, 6, 8 (first, second, fourth, third).
void CoarseSplitPass::quad(const SplitSector &sector, const SplitPolygon &polygon) {
  const PackedIndices vertices(memory_.r32(polygon.record + polygon::kIndices));
  const GridPoint p0 = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint p1 = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint p3 = unpackGridVertex(frame_, sector, vertices.fourth);
  const GridPoint p2 = unpackGridVertex(frame_, sector, vertices.third);
  const std::uint32_t links = memory_.r32(polygon.record + polygon::kLinks);
  const std::uint32_t draw = memory_.r32(polygon.record + polygon::kDraw);

  // 80025F80.
  const PackedIndices colourIndices(memory_.r32(polygon.record + polygon::kColours));
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t page = memory_.r32(texture + 4);
  const std::uint32_t code = asSigned(page) > 0 ? kOpaqueQuadCode : kSemiQuadCode;
  std::array<std::uint32_t, 4> colours = {memory_.r32(sector.colours + colourIndices.first) + code,
                                          memory_.r32(sector.colours + colourIndices.second) + code,
                                          memory_.r32(sector.colours + colourIndices.fourth) + code,
                                          memory_.r32(sector.colours + colourIndices.third) + code};
  loadCornerColours(frame_, colours.data(), 4);
  grid::storeCoarseQuad(frame_, {p0, p1, p3, p2}, colours);

  projectGrid(frame_, grid::kCoarseQuadCells, false);
  grid::refineCoarseCentre(frame_);
  const std::uint32_t uv = memory_.r32(texture);
  grid::drawEdgeCracks(
      frame_,
      {.edges = quadNeighbourEdges(frame_, links, draw, 1),
       .table = frame_.facts.coarse.quadCracks},
      {.colourAdjust = kGt4ToGt3, .uv = uv, .page = page, .binBias = binBias(draw)});
  drawSubQuads(polygon, draw);
}

// 80026438: the four GT4s take their texture from the record's next word pair.
void CoarseSplitPass::drawSubQuads(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  for (std::uint32_t k = 0; k < 4; ++k) {
    const std::uint32_t first = memory_.r32(frame_.facts.coarse.quadCells + 4 * k);
    frame_.v1 = first;
    const std::uint32_t pair = texture + 8 * k + 0x10;
    const std::uint32_t c0 = memory_.r32(first + cell::kDepth);
    const std::uint32_t c1 = memory_.r32(first + kCellSize + cell::kDepth);
    const std::uint32_t c3 = memory_.r32(first + 3 * kCellSize + cell::kDepth);
    const std::uint32_t c4 = memory_.r32(first + 4 * kCellSize + cell::kDepth);
    const std::uint32_t uv = memory_.r32(pair);
    if ((c0 & c1 & c3 & c4 & kOutcodeMask) != 0) {
      continue;
    }
    const std::uint32_t page = memory_.r32(pair + 4);
    const std::uint32_t sum =
        (c0 & kDepthMask) + (c1 & kDepthMask) + (c3 & kDepthMask) + (c4 & kDepthMask);
    const std::uint32_t xy0 = memory_.r32(first + cell::kScreen);
    const std::uint32_t xy1 = memory_.r32(first + kCellSize + cell::kScreen);
    const std::uint32_t xy3 = memory_.r32(first + 3 * kCellSize + cell::kScreen);
    const std::uint32_t xy4 = memory_.r32(first + 4 * kCellSize + cell::kScreen);
    const std::uint32_t prim = frame_.primitive;
    if (polygon.overflow &&
        !facesCamera(frame_, {xy0, xy1, xy3, xy4}, sidedness(draw), SecondTriangle::FourthFirst)) {
      memory_.w32(prim + packet::kXy0, xy0);
      continue;
    }
    memory_.w32(prim + packet::kXy0, xy0);
    memory_.w32(prim + packet::kXy1, xy1);
    memory_.w32(prim + packet::kXy2, xy3);
    memory_.w32(prim + packet::kXy3, xy4);
    memory_.w32(prim + packet::kColour0, memory_.r32(first + cell::kColour));
    memory_.w32(prim + packet::kColour1, memory_.r32(first + kCellSize + cell::kColour));
    memory_.w32(prim + packet::kColour2, memory_.r32(first + 3 * kCellSize + cell::kColour));
    memory_.w32(prim + packet::kColour3, memory_.r32(first + 4 * kCellSize + cell::kColour));
    writeQuadUvs(memory_, prim, {.uv = uv, .page = page}, frame_.facts.coarse.pageRemaps);
    memory_.w32(prim, kQuadTag);
    frame_.v1 = frame_.linkAndAdvance(polygonBin(sum, draw), PacketSize{kQuadBytes}).head;
  }
  frame_.v0 = frame_.facts.coarse.quadCells + 0x10;
}

// 80026AD0: the four GT3s, each a corner-dependent sub-texture of the record.
void CoarseSplitPass::drawSubTriangles(const SplitPolygon &polygon, std::uint32_t draw) {
  const std::uint32_t texture = textureRecord(frame_, draw);
  const std::uint32_t corner = (draw & kTriangleCornerMask) >> 12;
  const std::uint32_t subTextures = texture + 0x10;
  for (std::uint32_t k = 0; k < 4; ++k) {
    const std::uint32_t layout = memory_.r32(frame_.facts.coarse.triangleCells + 4 * k);
    const auto offset = static_cast<std::int8_t>(
        memory_.r8(frame_.facts.coarse.triangleSubTexture + (layout & 0xCu) + corner));
    const std::uint32_t c0 = kScratchpad + ((layout >> 20) & 0xFF0u);
    const std::uint32_t c1 = kScratchpad + ((layout >> 12) & 0xFF0u);
    const std::uint32_t c2 = kScratchpad + ((layout >> 4) & 0xFF0u);
    const std::uint32_t pair = subTextures + static_cast<std::uint32_t>(offset);
    const std::uint32_t page = memory_.r32(pair + 4) - kTexelSpan;
    const std::uint32_t uv = memory_.r32(pair);
    const std::uint32_t prim = frame_.primitive;
    writePieceTriangleUvs(memory_,
                          prim,
                          {.uv = uv, .page = page},
                          {.table = frame_.facts.coarse.triangleUvs, .turn = layout + corner},
                          kEnginePieceUvs);
    const std::uint32_t d0 = memory_.r32(c0 + cell::kDepth);
    const std::uint32_t d1 = memory_.r32(c1 + cell::kDepth);
    const std::uint32_t d2 = memory_.r32(c2 + cell::kDepth);
    frame_.v0 = d1;
    frame_.v1 = d2;
    if ((d0 & d1 & d2 & kOutcodeMask) != 0) {
      continue;
    }
    const std::uint32_t sum = (d0 & kDepthMask) + (d1 & kDepthMask) + 2 * (d2 & kDepthMask);
    const std::uint32_t xy0 = memory_.r32(c0 + cell::kScreen);
    const std::uint32_t xy1 = memory_.r32(c1 + cell::kScreen);
    const std::uint32_t xy2 = memory_.r32(c2 + cell::kScreen);
    frame_.v0 = xy1;
    frame_.v1 = xy2;
    if (polygon.overflow && !triangleFacesCamera(frame_, {xy0, xy1, xy2}, sidedness(draw))) {
      memory_.w32(prim + packet::kXy0, xy0);
      continue;
    }
    memory_.w32(prim + packet::kXy0, xy0);
    memory_.w32(prim + packet::kXy1, xy1);
    memory_.w32(prim + packet::kXy2, xy2);
    memory_.w32(prim + packet::kColour0, memory_.r32(c0 + cell::kColour));
    memory_.w32(prim + packet::kColour1, memory_.r32(c1 + cell::kColour));
    memory_.w32(prim + packet::kColour2, memory_.r32(c2 + cell::kColour));
    memory_.w32(prim, kTriangleTag);
    frame_.v0 = frame_.linkAndAdvance(polygonBin(sum, draw), PacketSize{kTriangleBytes}).head;
    frame_.v1 = prim >> 16;
  }
}

// 800265E4: a triangle cut into four. Its corners are cells 0, 2, 5.
void CoarseSplitPass::triangle(const SplitSector &sector, const SplitPolygon &polygon) {
  const PackedIndices vertices(memory_.r32(polygon.record + polygon::kIndices));
  const GridPoint p0 = unpackGridVertex(frame_, sector, vertices.first);
  const GridPoint p1 = unpackGridVertex(frame_, sector, vertices.second);
  const GridPoint p2 = unpackGridVertex(frame_, sector, vertices.third);
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
  grid::storeCoarseTriangle(frame_, {p0, p1, p2}, colours);

  projectGrid(frame_, grid::kCoarseTriangleCells, false);
  const std::uint32_t uv = memory_.r32(texture);
  const std::uint32_t corner = draw & kTriangleCornerMask;
  grid::drawEdgeCracks(
      frame_,
      {.edges = triangleNeighbourEdges(frame_, links, colourWord, 1),
       .table = frame_.facts.coarse.triangleCracks + (corner >> 7) + (corner >> 10)},
      {.colourAdjust = 0, .uv = uv, .page = page, .binBias = binBias(draw)});
  drawSubTriangles(polygon, draw);
}

} // namespace

void splitCoarsePolygons(TerrainFrame &frame) {
  CoarseSplitPass(frame).run();
}

} // namespace spyro::guest_terrain
