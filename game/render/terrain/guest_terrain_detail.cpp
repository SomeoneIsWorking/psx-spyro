// The two near passes of the terrain drawer (SCUS_944.25 80024534..80025CC0).

#include "guest_terrain_passes.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_render_globals.h"
#include "guest_terrain_fog.h"
#include "guest_terrain_frame.h"
#include "guest_terrain_mesh.h"
#include "guest_terrain_polygon.h"
#include "guest_word.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

// Thresholds are on the sum of a polygon's four depths; a triangle counts its third twice.
constexpr std::uint32_t kSplitSum = 0x2000;
constexpr std::uint32_t kPlainSum = 0x7C00;
constexpr std::uint32_t kDropSum = 0x8800;
constexpr std::uint32_t kFineVertexDepth = 0x140;
constexpr std::uint32_t kFadeDepth = 0x1F00; // a vertex at or past this is wholly its far colour
constexpr std::uint32_t kFullBright = 0x1000;
constexpr std::uint32_t kBrightest = 0xFFF;
constexpr std::uint32_t kNearRecord = 8; // the record's second word pair: full-bright texture

// 80024C94: `fade` is 0x1F00 - depth.
enum class Shade : std::uint8_t {
  Near,
  Far,
  Faded,
};

// One vertex's index into the sector's colour tables and its depth.
struct VertexShade {
  std::uint32_t colourIndex = 0;
  std::uint32_t depth = 0;
};

// What the last vertex decides for the primitive: the texture record step and the CLUT darkening.
struct PrimitiveFinish {
  std::uint32_t recordStep = 0;
  std::uint32_t darken = 0;
};

struct ShadedVertex {
  std::uint32_t colour;
  std::uint32_t fade;
  Shade shade;
};

class DetailPass {
public:
  explicit DetailPass(TerrainFrame &frame)
      : frame_(frame), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  void drawSector(std::uint32_t listWord);
  void drawQuad(const PackedIndices &vertices);
  void drawTriangle(const PackedIndices &vertices);
  [[nodiscard]] std::uint32_t screenWord(std::uint32_t index) const;
  [[nodiscard]] std::uint32_t depth(std::uint32_t index) const;
  void defer(std::span<const std::uint32_t> depths, std::uint32_t entry);
  ShadedVertex shadeVertex(const VertexShade &input);
  void shadeMiddle(const ShadedVertex &vertex);
  PrimitiveFinish finishPrimitive(const VertexShade &last, std::uint32_t colourSlot);

  TerrainFrame &frame_;
  Core &core_;
  guest_memory::WorkingMemory &memory_;
  std::uint32_t list_ = 0;
  std::uint32_t fog_ = 0;
  std::uint32_t sectorWord_ = 0; // the sector address, plus the split headers it has written
  std::uint32_t flagCursor_ = 0; // one past the polygon's flag byte; sign clear = no clipping
  std::uint32_t nearColours_ = 0;
  std::uint32_t farColours_ = 0;
  bool overflowOutcodes_ = false; // the vertex words carry overflow bits
  std::uint32_t record_ = 0;      // one past the current polygon record
  std::uint32_t brightest_ = 0;   // the brightest fade of the primitive so far
};

void DetailPass::run() {
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    core_.gte().writeControl(gte::kRotation + i, frame_.rotationWord(i));
  }
  core_.gte().writeControl(gte::kLight3, memory_.r32(frame_.facts.detail.fogLevel));
  frame_.primitive = frame_.primitiveBase();
  frame_.orderingTable = memory_.r32(frame_.globals.orderingTable);
  list_ = frame_.scratch + kDetailList;
  frame_.fineSplitCursor = frame_.scratch + kFineSplitList;
  frame_.coarseSplitCursor = frame_.scratch + kCoarseSplitList;
  frame_.textures = memory_.r32(frame_.facts.detail.textureTable);
  const std::uint32_t x = frame_.positionWord(0) >> 2;
  frame_.v0 = frame_.positionWord(1) >> 2;
  frame_.v1 = frame_.positionWord(2) >> 2;
  core_.gte().writeControl(gte::kLight0, x);
  core_.gte().writeControl(gte::kLight1, frame_.v0);
  core_.gte().writeControl(gte::kLight2, frame_.v1);
  for (;;) {
    const std::uint32_t word = memory_.r32(list_);
    list_ += 4;
    if (word == 0) {
      return;
    }
    drawSector(word);
  }
}

// 800245D0.
void DetailPass::drawSector(std::uint32_t listWord) {
  const std::uint32_t sectorClass = listWord & kSectorClassMask;
  sectorWord_ = listWord ^ sectorClass;
  const std::uint32_t sector = sectorWord_;
  flagCursor_ = frame_.scratch + (memory_.r32(sector + near_sector::kOriginY) & 0xFFFFu);
  const ProjectedSector projected = projectSector(frame_, listWord, NearPass::Detail);
  if (!projected.drawn) {
    return;
  }
  if (!projected.clipped) {
    flagCursor_ = (flagCursor_ << 1) >> 1;
  }

  // 80024948: the colour arrays, fogged first when the level fogs and the sector allows it.
  const std::uint32_t layout = memory_.r32(sector + near_sector::kLayout);
  const std::uint32_t colours = projected.vertexEnd - 8;
  const std::uint32_t colourBytes = (layout >> 6) & 0x3FCu;
  const std::uint32_t records = colours + 2 * colourBytes;
  fog_ = core_.gte().readControl(gte::kLight3);
  nearColours_ = colours;
  if (fog_ != 0) {
    const std::uint32_t fogFlag = memory_.r32(sector + near_sector::kFog);
    const std::uint32_t end = colours + 4 + 2 * colourBytes;
    frame_.v1 = fogFlag;
    frame_.v0 = end;
    if (asSigned(fogFlag) >= 0) {
      frame_.v1 = fogColours(memory_, core_, frame_.facts, colours, end, fog_);
      nearColours_ = frame_.facts.foggedColours;
    }
  }
  farColours_ = nearColours_ + colourBytes;
  overflowOutcodes_ = asSigned(nearColours_ ^ (projected.overflowed ? 0x80000000u : 0u)) >= 0;

  record_ = records;
  const std::uint32_t recordsEnd = ((layout >> 12) & 0xFF0u) + records;
  std::uint32_t indices = memory_.r32(record_);
  while (record_ != recordsEnd) {
    const PackedIndices vertices(indices);
    frame_.v0 = vertices.second;
    frame_.v1 = vertices.third;
    indices = memory_.r32(record_ + polygon::kSize);
    record_ += polygon::kSize;
    if (vertices.triangle()) {
      drawTriangle(vertices);
    } else {
      drawQuad(vertices);
    }
  }
}

std::uint32_t DetailPass::screenWord(std::uint32_t index) const {
  return memory_.r32(kScratchpad + index);
}

std::uint32_t DetailPass::depth(std::uint32_t index) const {
  return memory_.r16(kVertexDepths + (index >> 1));
}

// 80024B90 / 80025080: a polygon too near for one primitive. `entry` is the split-list entry.
void DetailPass::defer(std::span<const std::uint32_t> depths, std::uint32_t entry) {
  const bool fine = std::any_of(depths.begin(), depths.end(), [](std::uint32_t depth) {
    return depth < kFineVertexDepth;
  });
  if (!fine) {
    deferToSplitList(memory_,
                     {.cursor = frame_.coarseSplitCursor, .headerBit = kCoarseHeaderWritten},
                     sectorWord_,
                     {.header = sectorWord_ >> 2, .entry = entry});
    memory_.w8(flagCursor_ - 1, 2);
    return;
  }
  // Retail tests the depth sum; it is zero exactly when every depth is.
  if (std::all_of(depths.begin(), depths.end(), [](std::uint32_t depth) {
        return depth == 0;
      })) {
    return;
  }
  deferToSplitList(memory_,
                   {.cursor = frame_.fineSplitCursor, .headerBit = kFineHeaderWritten},
                   sectorWord_,
                   {.header = sectorWord_ >> 2, .entry = entry});
}

// 80024C84: the near colour is loaded into the GTE's FC registers as the fade's far colour.
ShadedVertex DetailPass::shadeVertex(const VertexShade &input) {
  const auto [colourIndex, depth] = input;
  const std::uint32_t nearColour = memory_.r32(nearColours_ + colourIndex);
  const std::uint32_t fade = kFadeDepth - depth;
  if (asSigned(fade - kFullBright) >= 0) {
    return ShadedVertex{nearColour, fade, Shade::Near};
  }
  const std::uint32_t farColour = memory_.r32(farColours_ + colourIndex);
  if (asSigned(fade) <= 0) {
    return ShadedVertex{farColour, fade, Shade::Far};
  }
  return ShadedVertex{
      fadeColour(core_, {.nearColour = nearColour, .farColour = farColour, .fade = fade}),
      fade,
      Shade::Faded};
}

// A vertex after the first and before the last: track the brightest fade.
void DetailPass::shadeMiddle(const ShadedVertex &vertex) {
  if (vertex.shade == Shade::Near) {
    brightest_ = kBrightest;
  } else if (vertex.shade == Shade::Faded && asSigned(vertex.fade - brightest_) >= 0) {
    brightest_ = vertex.fade;
  }
}

// 80024DB8 / 80025234: a primitive with a full-bright vertex uses the record's second
// (full-bright) pair; every other one darkens its CLUT row by how far its brightest vertex is
// from full.
PrimitiveFinish DetailPass::finishPrimitive(const VertexShade &last, std::uint32_t colourSlot) {
  const ShadedVertex vertex = shadeVertex(last);
  memory_.w8(flagCursor_ - 1, 1);
  PrimitiveFinish finish;
  if (vertex.shade == Shade::Near) {
    finish.recordStep = kNearRecord;
  } else {
    if (vertex.shade == Shade::Faded && asSigned(vertex.fade - brightest_) >= 0) {
      brightest_ = vertex.fade;
    }
    finish.darken = 0xFu - (brightest_ >> 8);
    if (vertex.shade == Shade::Faded && finish.darken == 0) {
      finish.recordStep = kNearRecord;
    }
  }
  memory_.w32(frame_.primitive + colourSlot, vertex.colour);
  return finish;
}

// 80024A28.
void DetailPass::drawQuad(const PackedIndices &vertices) {
  const bool clipped = asSigned(flagCursor_) <= 0;
  flagCursor_ += 1;
  const auto corners = unclipCorners<4>({screenWord(vertices.first),
                                         screenWord(vertices.second),
                                         screenWord(vertices.third),
                                         screenWord(vertices.fourth)},
                                        clipped,
                                        overflowOutcodes_);
  if (!corners) {
    return;
  }
  const auto [xy, overflow] = *corners;
  const std::uint32_t draw = memory_.r32(record_ - polygon::kSize + polygon::kDraw);
  if (!nearQuadFacesCamera(frame_, {xy[0], xy[1], xy[3], xy[2]}, sidedness(draw), overflow != 0)) {
    frame_.v0 = depth(vertices.second);
    frame_.v1 = depth(vertices.third);
    return;
  }
  const std::uint32_t depths[4] = {
      depth(vertices.first), depth(vertices.second), depth(vertices.third), depth(vertices.fourth)};
  frame_.v0 = depths[1];
  frame_.v1 = depths[2];
  const std::uint32_t sum = depths[0] + depths[1] + depths[2] + depths[3];
  if (sum < kSplitSum) {
    defer(depths, record_ - polygon::kSize + (overflow >> 4));
    return;
  }
  if (sum >= kPlainSum) {
    if (sum >= kDropSum) {
      return;
    }
    if (depths[0] >= kFadeDepth && depths[1] >= kFadeDepth && depths[2] >= kFadeDepth &&
        depths[3] >= kFadeDepth) {
      return;
    }
  }

  // 80024C48: one GT4.
  const std::uint32_t prim = frame_.primitive;
  const PackedIndices colours(memory_.r32(record_ - polygon::kSize + polygon::kColours));
  const std::uint32_t bin = polygonBin(sum, draw);
  memory_.w32(prim + packet::kXy0, xy[0]);
  memory_.w32(prim + packet::kXy1, xy[1]);
  memory_.w32(prim + packet::kXy2, xy[3]);
  memory_.w32(prim + packet::kXy3, xy[2]);
  std::uint32_t record = (draw & kTextureRecordMask) * kTextureRecordSize;

  const ShadedVertex first = shadeVertex({colours.first, depths[0]});
  memory_.w32(prim + packet::kColour0, first.colour | kOpaqueQuadCode);
  brightest_ = first.shade == Shade::Near  ? kBrightest
               : first.shade == Shade::Far ? 0u
                                           : first.fade;
  const ShadedVertex second = shadeVertex({colours.second, depths[1]});
  memory_.w32(prim + packet::kColour1, second.colour);
  shadeMiddle(second);
  const ShadedVertex third = shadeVertex({colours.third, depths[2]});
  memory_.w32(prim + packet::kColour3, third.colour);
  shadeMiddle(third);
  const PrimitiveFinish finish = finishPrimitive({colours.fourth, depths[3]}, packet::kColour2);

  record += finish.recordStep + frame_.textures;
  const std::uint32_t uv = memory_.r32(record) + (finish.darken << 22);
  const std::uint32_t page = memory_.r32(record + 4);
  memory_.w32(prim, kQuadTag);
  memory_.w32(prim + packet::kUv0, uv);
  if (asSigned(page) < 0) {
    memory_.w8(prim + packet::kCode, static_cast<std::uint8_t>(kSemiQuadCode >> 24));
  }
  memory_.w32(prim + packet::kUv1, page - 0x1F00u);
  memory_.w32(prim + packet::kUv2, uv + 0x1F00u);
  memory_.w16(prim + packet::kUv3, static_cast<std::uint16_t>(page));
  frame_.v1 = uv + 0x1F00u;
  frame_.v0 = prim >> 16;
  frame_.linkAndAdvance(bin, PacketSize{kQuadBytes});
}

// 80024F8C.
void DetailPass::drawTriangle(const PackedIndices &vertices) {
  const bool clipped = asSigned(flagCursor_) <= 0;
  flagCursor_ += 1;
  const auto corners = unclipCorners<3>(
      {screenWord(vertices.first), screenWord(vertices.second), screenWord(vertices.third)},
      clipped,
      overflowOutcodes_);
  if (!corners) {
    return;
  }
  const auto [xy, overflow] = *corners;
  const std::uint32_t draw = memory_.r32(record_ - polygon::kSize + polygon::kDraw);
  if (!nearTriangleFacesCamera(frame_, xy, sidedness(draw), overflow != 0)) {
    frame_.v0 = kVertexDepths + (vertices.second >> 1);
    frame_.v1 = kVertexDepths + (vertices.third >> 1);
    return;
  }
  const std::uint32_t depths[3] = {
      depth(vertices.first), depth(vertices.second), depth(vertices.third)};
  frame_.v0 = depths[1];
  frame_.v1 = depths[2];
  const std::uint32_t sum = depths[0] + depths[1] + 2 * depths[2];
  if (sum < kSplitSum) {
    defer(depths, (record_ - polygon::kSize + (overflow >> 4)) | kSplitTriangle);
    return;
  }
  if (sum >= kPlainSum) {
    if (sum >= kDropSum) {
      return;
    }
    if (depths[0] >= kFadeDepth && depths[1] >= kFadeDepth && depths[2] >= kFadeDepth) {
      return;
    }
  }

  // 80025130: one GT3.
  const std::uint32_t prim = frame_.primitive;
  const PackedIndices colours(memory_.r32(record_ - polygon::kSize + polygon::kColours));
  const std::uint32_t bin = polygonBin(sum, draw);
  memory_.w32(prim + packet::kXy0, xy[0]);
  memory_.w32(prim + packet::kXy1, xy[1]);
  memory_.w32(prim + packet::kXy2, xy[2]);
  std::uint32_t record = (draw & kTextureRecordMask) * kTextureRecordSize;

  const ShadedVertex first = shadeVertex({colours.first, depths[0]});
  memory_.w32(prim + packet::kColour0, first.colour | kOpaqueTriangleCode);
  brightest_ = first.shade == Shade::Near  ? kBrightest
               : first.shade == Shade::Far ? 0u
                                           : first.fade;
  const ShadedVertex second = shadeVertex({colours.second, depths[1]});
  memory_.w32(prim + packet::kColour1, second.colour);
  shadeMiddle(second);
  const PrimitiveFinish finish = finishPrimitive({colours.third, depths[2]}, packet::kColour2);

  record += finish.recordStep + frame_.textures;
  const std::uint32_t uv = memory_.r32(record) + (finish.darken << 22);
  const std::uint32_t page = memory_.r32(record + 4);
  memory_.w32(prim, kTriangleTag);
  if (asSigned(page) < 0) {
    memory_.w8(prim + packet::kCode, static_cast<std::uint8_t>(kSemiTriangleCode >> 24));
  }
  frame_.v1 = writeTriangleUvs(memory_, prim, {.uv = uv, .page = page}, draw & kTriangleCornerMask);
  frame_.v0 = prim >> 16;
  frame_.linkAndAdvance(bin, PacketSize{kTriangleBytes});
}

} // namespace

void drawDetailSectors(TerrainFrame &frame) {
  DetailPass(frame).run();
}

} // namespace spyro::guest_terrain
