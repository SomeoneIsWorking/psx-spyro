// guest_terrain_translucent.cpp — the translucent pass of the terrain drawer of this engine family
// (SCUS_944.25 80025434): see guest_terrain_detail.cpp, whose shape it shares.

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_frame.h"
#include "guest_terrain_mesh.h"
#include "guest_terrain_passes.h"
#include "guest_terrain_polygon.h"
#include "guest_word.h"

namespace spyro::guest_terrain {
namespace {

// The GTE register and command numbers are shared vocabulary (psxport gte_registers.h), not this
// pass's.
namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kSplitSum = 0x2000;
constexpr std::uint32_t kDropSum = 0x6000;
constexpr std::uint32_t kFineVertexDepth = 0x140;
constexpr std::uint32_t kFadeStart = 0x1000; // nearer than this keeps the sector colour
constexpr std::uint32_t kFadeOverflow = 0x3000;
constexpr std::uint32_t kFadeFull = 0xFFF;

// 80025918: the fade toward black (FC = 0) of a vertex at `depth`: none until 0x1000, then twice
// the distance past it, with retail's clamp, which tests two bits rather than comparing.
std::uint32_t fadeByDepth(std::uint32_t depth) {
  const std::uint32_t past = depth - kFadeStart;
  const std::uint32_t fade = asSigned(past) >= 0 ? past << 1 : 0u;
  return (fade & kFadeOverflow) != 0 ? kFadeFull : fade;
}

class TranslucentPass {
public:
  explicit TranslucentPass(TerrainFrame &frame)
      : frame_(frame), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  void drawSector(std::uint32_t listWord);
  void drawQuad(const PackedIndices &vertices);
  void drawTriangle(const PackedIndices &vertices);
  [[nodiscard]] std::uint32_t screenWord(std::uint32_t index) const;
  [[nodiscard]] std::uint32_t depth(std::uint32_t index) const;
  // Which split list a too-near polygon goes to; `None` is a fine polygon whose depths are all
  // zero.
  enum class Split : std::uint8_t { Coarse, Fine, None };
  static Split split(bool fine, std::uint32_t sum) {
    if (!fine) {
      return Split::Coarse;
    }
    return sum == 0 ? Split::None : Split::Fine;
  }

  void defer(Split split, std::uint32_t entry);
  std::uint32_t fadedColour(std::uint32_t depth);

  TerrainFrame &frame_;
  Core &core_;
  guest_memory::WorkingMemory &memory_;
  std::uint32_t sectorWord_ = 0; // t9: the sector address, plus the split headers it has written
  bool clipped_ = false;         // s6
  std::uint32_t record_ = 0;     // s1: one past the current polygon record
};

// 80025434.
void TranslucentPass::run() {
  std::uint32_t list = frame_.scratch + kTranslucentList;
  for (;;) {
    const std::uint32_t word = memory_.r32(list);
    list += 4;
    if (word == 0) {
      break;
    }
    drawSector(word);
  }
  // 80025CC0: terminate the split lists for the subdivision passes.
  memory_.w32(frame_.coarseSplitCursor, 0);
  memory_.w32(frame_.fineSplitCursor, 0);
}

// 80025440.
void TranslucentPass::drawSector(std::uint32_t listWord) {
  const std::uint32_t sectorClass = listWord & kSectorClassMask;
  sectorWord_ = listWord ^ sectorClass;
  const std::uint32_t sector = sectorWord_;
  const ProjectedSector projected = projectSector(frame_, listWord, NearPass::Translucent);
  if (!projected.drawn) {
    return;
  }
  clipped_ = projected.clipped;

  // 80025780: one colour for the whole sector, faded toward black.
  const std::uint32_t layout = memory_.r32(sector + near_sector::kLayout);
  const std::uint32_t colours = projected.vertexEnd - 8;
  const std::uint32_t colour = memory_.r32(colours);
  record_ = colours + 2 * ((layout >> 6) & 0x3FCu);
  const std::uint32_t recordsEnd = ((layout >> 12) & 0xFF0u) + record_;
  std::uint32_t indices = memory_.r32(record_);
  core_.gte().writeControl(gte::kFarRed, 0);
  core_.gte().writeControl(gte::kFarGreen, 0);
  core_.gte().writeControl(gte::kFarBlue, 0);
  core_.gte().writeData(gte::kRgbc, colour);
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

std::uint32_t TranslucentPass::screenWord(std::uint32_t index) const {
  return memory_.r32(kScratchpad + index);
}

std::uint32_t TranslucentPass::depth(std::uint32_t index) const {
  return memory_.r16(kVertexDepths + (index >> 1));
}

// 80025884: the split lists, under a header that is the sector address / 4 with its top bit
// cleared.
void TranslucentPass::defer(Split split, std::uint32_t entry) {
  const std::uint32_t header = (sectorWord_ << 1) >> 3;
  if (split == Split::Coarse) {
    deferToSplitList(memory_,
                     {.cursor = frame_.coarseSplitCursor, .headerBit = kCoarseHeaderWritten},
                     sectorWord_,
                     {.header = header, .entry = entry});
    return;
  }
  if (split == Split::None) {
    return;
  }
  deferToSplitList(memory_,
                   {.cursor = frame_.fineSplitCursor, .headerBit = kFineHeaderWritten},
                   sectorWord_,
                   {.header = header, .entry = entry});
}

std::uint32_t TranslucentPass::fadedColour(std::uint32_t depth) {
  core_.gte().writeData(gte::kIr0, fadeByDepth(depth));
  core_.gte().execute(gte::kDpcs);
  return core_.gte().readData(gte::kRgb2);
}

// 800257F0.
void TranslucentPass::drawQuad(const PackedIndices &vertices) {
  const auto corners = unclipCorners<4>({screenWord(vertices.first),
                                         screenWord(vertices.second),
                                         screenWord(vertices.third),
                                         screenWord(vertices.fourth)},
                                        clipped_,
                                        false);
  if (!corners) {
    return;
  }
  const std::array<std::uint32_t, 4> &xy = corners->xy;
  const std::uint32_t draw = memory_.r32(record_ - polygon::kSize + polygon::kDraw);
  const std::uint32_t d0 = depth(vertices.first);
  const std::uint32_t d1 = depth(vertices.second);
  const std::uint32_t d2 = depth(vertices.third);
  const std::uint32_t d3 = depth(vertices.fourth);
  frame_.v0 = d1;
  frame_.v1 = d2;
  const std::uint32_t sum = d0 + d1 + d2 + d3;
  if (sum < kSplitSum) {
    const bool fine = d0 < kFineVertexDepth || d1 < kFineVertexDepth || d2 < kFineVertexDepth ||
                      d3 < kFineVertexDepth;
    defer(split(fine, sum), record_ - polygon::kSize);
    return;
  }
  if (sum >= kDropSum) {
    return;
  }

  // 80025908: one semi-transparent GT4. Retail pairs the third vertex's colour with the fourth's
  // position and the fourth's colour with the third's (the detail pass pairs them correctly).
  const std::uint32_t prim = frame_.primitive;
  memory_.w32(prim + packet::kXy0, xy[0]);
  memory_.w32(prim + packet::kXy1, xy[1]);
  memory_.w32(prim + packet::kXy2, xy[3]);
  memory_.w32(prim + packet::kXy3, xy[2]);
  const std::uint32_t c0 = fadedColour(d0);
  const std::uint32_t c1 = fadedColour(d1);
  const std::uint32_t c2 = fadedColour(d2);
  memory_.w32(prim + packet::kColour0, c0 | kSemiQuadCode);
  memory_.w32(prim + packet::kColour1, c1);
  memory_.w32(prim + packet::kColour2, c2);
  const std::uint32_t c3 = fadedColour(d3);
  memory_.w32(prim + packet::kColour3, c3);
  const std::uint32_t bin = polygonBin(sum, draw);
  const std::uint32_t record = textureRecord(frame_, draw);
  const std::uint32_t uv = memory_.r32(record);
  const std::uint32_t page = memory_.r32(record + 4);
  memory_.w32(prim, kQuadTag);
  memory_.w32(prim + packet::kUv0, uv);
  memory_.w32(prim + packet::kUv1, page - 0x1F00u);
  memory_.w32(prim + packet::kUv2, uv + 0x1F00u);
  memory_.w16(prim + packet::kUv3, static_cast<std::uint16_t>(page));
  frame_.v1 = uv + 0x1F00u;
  frame_.v0 = prim >> 16;
  frame_.linkAndAdvance(bin, PacketSize{kQuadBytes});
}

// 80025A4C.
void TranslucentPass::drawTriangle(const PackedIndices &vertices) {
  const auto corners = unclipCorners<3>(
      {screenWord(vertices.first), screenWord(vertices.second), screenWord(vertices.third)},
      clipped_,
      false);
  if (!corners) {
    return;
  }
  const std::array<std::uint32_t, 3> &xy = corners->xy;
  const std::uint32_t draw = memory_.r32(record_ - polygon::kSize + polygon::kDraw);
  const std::uint32_t d0 = depth(vertices.first);
  const std::uint32_t d1 = depth(vertices.second);
  const std::uint32_t d2 = depth(vertices.third);
  frame_.v0 = d1;
  frame_.v1 = d2;
  const std::uint32_t sum = d0 + d1 + 2 * d2;
  if (sum < kSplitSum) {
    // 80025AC4: retail's fourth test reads the fourth vertex INDEX, not a depth (the triangle path
    // never loads one), so a triangle whose index is under 0x140 / 4 always splits fine.
    const bool fine = d0 < kFineVertexDepth || d1 < kFineVertexDepth || d2 < kFineVertexDepth ||
                      vertices.fourth < kFineVertexDepth;
    defer(split(fine, sum), (record_ - polygon::kSize) | kSplitTriangle);
    return;
  }
  if (sum >= kDropSum) {
    return;
  }

  // 80025B50: one semi-transparent GT3.
  const std::uint32_t prim = frame_.primitive;
  memory_.w32(prim + packet::kXy0, xy[0]);
  memory_.w32(prim + packet::kXy1, xy[1]);
  memory_.w32(prim + packet::kXy2, xy[2]);
  const std::uint32_t c0 = fadedColour(d0);
  const std::uint32_t c1 = fadedColour(d1);
  memory_.w32(prim + packet::kColour0, c0 | kSemiTriangleCode);
  memory_.w32(prim + packet::kColour1, c1);
  const std::uint32_t c2 = fadedColour(d2);
  memory_.w32(prim + packet::kColour2, c2);
  const std::uint32_t bin = polygonBin(sum, draw);
  const std::uint32_t record = textureRecord(frame_, draw);
  const std::uint32_t uv = memory_.r32(record);
  const std::uint32_t page = memory_.r32(record + 4);
  memory_.w32(prim, kTriangleTag);
  frame_.v1 = writeTriangleUvs(memory_, prim, {.uv = uv, .page = page}, draw & kTriangleCornerMask);
  frame_.v0 = prim >> 16;
  frame_.linkAndAdvance(bin, PacketSize{kTriangleBytes});
}

} // namespace

void drawTranslucentSectors(TerrainFrame &frame) {
  TranslucentPass(frame).run();
}

} // namespace spyro::guest_terrain
