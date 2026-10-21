#include "spyro1/render/terrain_split.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_polygon.h"

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
namespace polygon = spyro::guest_terrain::polygon;
namespace packet = spyro::guest_terrain::packet;
using spyro::guest_terrain::PackedIndices;

using spyro::guest_word::asSigned;

// A neighbour's flag byte at the signed 12-bit offset in `word`'s top (`high`) or next 12 bits.
std::uint32_t offsetHigh(std::uint32_t word) {
  return static_cast<std::uint32_t>(asSigned(word) >> 20);
}
std::uint32_t offsetLow(std::uint32_t word) {
  return static_cast<std::uint32_t>(asSigned(word << 12) >> 20);
}

} // namespace

SplitPass::SplitPass(TerrainFrame &frame, const Facts &facts)
    : frame_(frame), facts_(facts),
      textures_(frame.memory.mem_r32(facts.lodTable + lod::kSplitTextures)) {}

SplitSector SplitPass::sector(std::uint32_t header) {
  const psx::present::EmitMemory &memory = frame_.memory;
  Core &core = frame_.core;
  const std::uint32_t sector = header << 2;
  const std::uint32_t zx = memory.mem_r32(sector + 0x08);
  const std::uint32_t y = memory.mem_r32(sector + 0x0C);
  const std::uint32_t layout = memory.mem_r32(sector + 0x14);
  SplitSector result;
  result.originZ = (zx >> 14) - core.gte().readControl(gte::kBackRed);
  result.originX = core.gte().readControl(gte::kBackGreen) - ((zx & 0xFFFFu) << 2);
  result.originY = core.gte().readControl(gte::kBackBlue) - (y >> 14);
  result.heightShift = 2;
  result.heightMask = 0xFFC;
  result.vertices = sector + 0x1C + ((layout >> 22) & 0x3FCu);
  result.colours = result.vertices + ((layout << 2) & 0x3FCu);
  flagBase_ = result.colours + ((layout >> 5) & 0x7F8u) - ((y & 0xFFFFu) << 4);
  frame_.v0 = layout;
  frame_.v1 = (y & 0xFFFFu) << 4;
  return result;
}

SplitRecord SplitPass::record(const SplitPolygon &polygon) const {
  const psx::present::EmitMemory &memory = frame_.memory;
  SplitRecord result;
  result.address = polygon.record;
  result.indices = memory.mem_r32(polygon.record + polygon::kIndices);
  result.colours = memory.mem_r32(polygon.record + polygon::kColours);
  result.links = memory.mem_r32(polygon.record + polygon::kLinks);
  result.draw = memory.mem_r32(polygon.record + polygon::kDraw);
  result.texture = textures_ + (result.links & 0x7Fu) * kSplitTextureSize;
  result.overflow = polygon.overflow;
  return result;
}

std::array<std::uint32_t, 4> SplitPass::quadColours(const SplitSector &sector,
                                                    const SplitRecord &record) const {
  const std::uint32_t code = (record.links & kSemiTransparent) != 0 ? spyro::guest_terrain::kSemiQuadCode
                                                                    : spyro::guest_terrain::kOpaqueQuadCode;
  const PackedIndices index(record.colours);
  const psx::present::EmitMemory &memory = frame_.memory;
  return {memory.mem_r32(sector.colours + index.first) + code,
          memory.mem_r32(sector.colours + index.second) + code,
          memory.mem_r32(sector.colours + index.fourth) + code,
          memory.mem_r32(sector.colours + index.third) + code};
}

std::array<std::uint32_t, 3> SplitPass::triangleColours(const SplitSector &sector,
                                                        const SplitRecord &record) const {
  const std::uint32_t code = (record.links & kSemiTransparent) != 0 ? spyro::guest_terrain::kSemiTriangleCode
                                                                    : spyro::guest_terrain::kOpaqueTriangleCode;
  const PackedIndices index(record.colours);
  const psx::present::EmitMemory &memory = frame_.memory;
  return {memory.mem_r32(sector.colours + index.first) + code,
          memory.mem_r32(sector.colours + index.second) + code,
          memory.mem_r32(sector.colours + index.third) + code};
}

// 80027BD0.
std::uint32_t SplitPass::quadEdges(const SplitRecord &record, std::uint32_t bit) const {
  const psx::present::EmitMemory &memory = frame_.memory;
  const std::uint32_t own = frame_.scratch + ((record.address - flagBase_) >> 4);
  const std::uint32_t first = memory.mem_r8(own + offsetHigh(record.links)) & bit;
  const std::uint32_t second = memory.mem_r8(own + offsetLow(record.links)) & bit;
  const std::uint32_t third = memory.mem_r8(own + offsetHigh(record.draw)) & bit;
  const std::uint32_t fourth = memory.mem_r8(own + offsetLow(record.draw)) & bit;
  return first | (second << 8) | (third << 16) | (fourth << 24);
}

// 80028240.
std::uint32_t SplitPass::triangleEdges(const SplitRecord &record, std::uint32_t bit) const {
  const psx::present::EmitMemory &memory = frame_.memory;
  const std::uint32_t own = frame_.scratch + ((record.address - flagBase_) >> 4);
  const std::uint32_t first = memory.mem_r8(own + offsetHigh(record.links)) & bit;
  const std::uint32_t second = memory.mem_r8(own + offsetLow(record.draw)) & bit;
  const std::uint32_t third = memory.mem_r8(own + offsetHigh(record.draw)) & bit;
  return first | (second << 8) | (third << 16);
}

spyro::guest_terrain::CrackPolygon SplitPass::crack(const SplitRecord &record,
                                                    std::uint32_t colourAdjust) const {
  return {.colourAdjust = colourAdjust,
          .uv = frame_.memory.mem_r32(record.texture),
          .page = frame_.memory.mem_r32(record.texture + 4),
          .binBias = binBias(record.draw)};
}

void SplitPass::writePieceUvs(const PieceTexture &texture) {
  const auto [pair, uvSpan] = texture;
  const psx::present::EmitMemory &memory = frame_.memory;
  const std::uint32_t uv = memory.mem_r32(pair);
  const std::uint32_t page = memory.mem_r32(pair + 4);
  std::uint32_t first = uv;
  std::uint32_t second = page;
  std::uint32_t third = uv + (uvSpan << 8);
  std::uint32_t fourth = third + uvSpan;
  const std::uint32_t remap = page >> 25;
  if (remap != 0) {
    const std::uint32_t rest = memory.mem_r32(facts_.pageRemaps + remap + 4);
    const std::uint32_t head = memory.mem_r32(facts_.pageRemaps + remap);
    third = uv + (rest & 0xFFFFu);
    fourth = uv + (rest >> 16);
    first = uv + (head & 0xFFFFu);
    second = page + static_cast<std::uint32_t>(asSigned(head) >> 16);
  }
  const std::uint32_t prim = frame_.primitive;
  memory.mem_w32(prim + packet::kUv0, first);
  memory.mem_w32(prim + packet::kUv1, second);
  memory.mem_w32(prim + packet::kUv2, third);
  memory.mem_w32(prim + packet::kUv3, fourth);
}

void SplitPass::writeTrianglePieceUvs(const SplitRecord &record,
                                      std::uint32_t layout,
                                      const TrianglePieces &pieces) {
  const psx::present::EmitMemory &memory = frame_.memory;
  const std::uint32_t corner = (record.links >> kCornerShift) & 3u;
  const auto offset = static_cast<std::int8_t>(
      memory.mem_r8(pieces.subTextures + (layout & pieces.selectMask) + corner));
  const std::uint32_t pair =
      record.texture + pieces.pairs + static_cast<std::uint32_t>(static_cast<std::int32_t>(offset));
  spyro::guest_terrain::writePieceTriangleUvs(
      memory,
      frame_.primitive,
      {.uv = memory.mem_r32(pair), .page = memory.mem_r32(pair + 4)},
      {.table = facts_.triangleUvs, .turn = corner + layout},
      kPieceUvs);
}

} // namespace spyro1::terrain
