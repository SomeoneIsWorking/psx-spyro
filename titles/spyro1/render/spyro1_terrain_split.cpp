#include "spyro1_terrain_split.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_polygon.h"
#include "guest_word.h"

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

constexpr std::uint32_t kGt4Code = 0x3C000000u;
constexpr std::uint32_t kGt3Code = 0x34000000u;
constexpr std::uint32_t kSemiCode = 0x02000000u;

} // namespace

Spyro1SplitPass::Spyro1SplitPass(TerrainFrame &frame, const Facts &facts)
    : frame_(frame), facts_(facts),
      textures_(frame.memory.r32(facts.lodTable + lod::kSplitTextures)) {}

SplitSector Spyro1SplitPass::sector(std::uint32_t header) {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  Core &core = frame_.core;
  const std::uint32_t sector = header << 2;
  const std::uint32_t zx = memory.r32(sector + 0x08);
  const std::uint32_t y = memory.r32(sector + 0x0C);
  const std::uint32_t layout = memory.r32(sector + 0x14);
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

SplitRecord Spyro1SplitPass::record(const SplitPolygon &polygon) const {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  SplitRecord result;
  result.address = polygon.record;
  result.indices = memory.r32(polygon.record + polygon::kIndices);
  result.colours = memory.r32(polygon.record + polygon::kColours);
  result.links = memory.r32(polygon.record + polygon::kLinks);
  result.draw = memory.r32(polygon.record + polygon::kDraw);
  result.texture = textures_ + (result.links & 0x7Fu) * kSplitTextureSize;
  result.overflow = polygon.overflow;
  return result;
}

std::array<std::uint32_t, 4> Spyro1SplitPass::quadColours(const SplitSector &sector,
                                                          const SplitRecord &record) const {
  const std::uint32_t code = kGt4Code + ((record.links & kSemiTransparent) != 0 ? kSemiCode : 0u);
  const PackedIndices index(record.colours);
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  return {memory.r32(sector.colours + index.first) + code,
          memory.r32(sector.colours + index.second) + code,
          memory.r32(sector.colours + index.fourth) + code,
          memory.r32(sector.colours + index.third) + code};
}

std::array<std::uint32_t, 3> Spyro1SplitPass::triangleColours(const SplitSector &sector,
                                                              const SplitRecord &record) const {
  const std::uint32_t code = kGt3Code + ((record.links & kSemiTransparent) != 0 ? kSemiCode : 0u);
  const PackedIndices index(record.colours);
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  return {memory.r32(sector.colours + index.first) + code,
          memory.r32(sector.colours + index.second) + code,
          memory.r32(sector.colours + index.third) + code};
}

// 80027BD0.
std::uint32_t Spyro1SplitPass::quadEdges(const SplitRecord &record, std::uint32_t bit) const {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const std::uint32_t own = frame_.scratch + ((record.address - flagBase_) >> 4);
  const std::uint32_t first = memory.r8(own + offsetHigh(record.links)) & bit;
  const std::uint32_t second = memory.r8(own + offsetLow(record.links)) & bit;
  const std::uint32_t third = memory.r8(own + offsetHigh(record.draw)) & bit;
  const std::uint32_t fourth = memory.r8(own + offsetLow(record.draw)) & bit;
  return first | (second << 8) | (third << 16) | (fourth << 24);
}

// 80028240.
std::uint32_t Spyro1SplitPass::triangleEdges(const SplitRecord &record, std::uint32_t bit) const {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const std::uint32_t own = frame_.scratch + ((record.address - flagBase_) >> 4);
  const std::uint32_t first = memory.r8(own + offsetHigh(record.links)) & bit;
  const std::uint32_t second = memory.r8(own + offsetLow(record.draw)) & bit;
  const std::uint32_t third = memory.r8(own + offsetHigh(record.draw)) & bit;
  return first | (second << 8) | (third << 16);
}

spyro::guest_terrain::CrackPolygon Spyro1SplitPass::crack(const SplitRecord &record,
                                                          std::uint32_t colourAdjust) const {
  return {.colourAdjust = colourAdjust,
          .uv = frame_.memory.r32(record.texture),
          .page = frame_.memory.r32(record.texture + 4),
          .binBias = binBias(record.draw)};
}

void Spyro1SplitPass::writePieceUvs(const PieceTexture &texture) {
  const auto [pair, uvSpan] = texture;
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const std::uint32_t uv = memory.r32(pair);
  const std::uint32_t page = memory.r32(pair + 4);
  std::uint32_t first = uv;
  std::uint32_t second = page;
  std::uint32_t third = uv + (uvSpan << 8);
  std::uint32_t fourth = third + uvSpan;
  const std::uint32_t remap = page >> 25;
  if (remap != 0) {
    const std::uint32_t rest = memory.r32(facts_.pageRemaps + remap + 4);
    const std::uint32_t head = memory.r32(facts_.pageRemaps + remap);
    third = uv + (rest & 0xFFFFu);
    fourth = uv + (rest >> 16);
    first = uv + (head & 0xFFFFu);
    second = page + static_cast<std::uint32_t>(asSigned(head) >> 16);
  }
  const std::uint32_t prim = frame_.primitive;
  memory.w32(prim + packet::kUv0, first);
  memory.w32(prim + packet::kUv1, second);
  memory.w32(prim + packet::kUv2, third);
  memory.w32(prim + packet::kUv3, fourth);
}

void Spyro1SplitPass::writeTrianglePieceUvs(const SplitRecord &record,
                                            std::uint32_t layout,
                                            const TrianglePieces &pieces) {
  spyro::guest_memory::WorkingMemory &memory = frame_.memory;
  const std::uint32_t corner = (record.links >> kCornerShift) & 3u;
  const auto offset = static_cast<std::int8_t>(
      memory.r8(pieces.subTextures + (layout & pieces.selectMask) + corner));
  const std::uint32_t pair =
      record.texture + pieces.pairs + static_cast<std::uint32_t>(static_cast<std::int32_t>(offset));
  spyro::guest_terrain::writePieceTriangleUvs(
      memory,
      frame_.primitive,
      {.uv = memory.r32(pair), .page = memory.r32(pair + 4)},
      {.table = facts_.triangleUvs, .turn = corner + layout},
      kPieceUvs);
}

} // namespace spyro1::terrain
