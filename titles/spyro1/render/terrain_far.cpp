// SCUS_942.28 80026200..80026788: the far pass. Every sector on the far list is drawn from its
// low-detail mesh as untextured Gouraud polygons (G3 / G4), each optionally preceded by a draw-mode
// packet. A sector touching a screen edge carries outcodes below its SXY << 5 (1 above, 2 below,
// 4 outside horizontally) and is skipped when every vertex shares one.
//
// Polygons are binned by mean depth and dropped past the level's far depth; those nearer than the
// near/far handover fade out over 0x20 bins, dropped once every corner is nearer than its bias.

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_far_vertices.h"
#include "render/terrain/guest_terrain_frame.h"
#include "spyro1/render/terrain_passes.h"

#include <array>

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
using spyro::guest_memory::WorkingMemory;
using spyro::guest_terrain::FarHeight;
using spyro::guest_terrain::kFarOutcodeShift;
using spyro::guest_terrain::kScratchpad;
using spyro::guest_terrain::Linked;
using spyro::guest_terrain::PacketSize;

using spyro::guest_word::asSigned;
using spyro::guest_word::sra;

namespace far_sector {
constexpr std::uint32_t kOrigin = 0x08;  // x origin high, y origin low
constexpr std::uint32_t kOriginZ = 0x0E; // halfword
constexpr std::uint32_t kLayout = 0x10;  // vertex count, colour bytes, record bytes
constexpr std::uint32_t kData = 0x1C;
} // namespace far_sector

constexpr std::uint32_t kEdgeTouching = 1;
constexpr std::uint32_t kRecordSize = 8;
constexpr std::uint32_t kQuadOutcodes = 0xF;
constexpr std::uint32_t kTriangleOutcodes = 0x1F;

// The record's first word: four cell offsets, two-sided, and the fade bias.
constexpr std::uint32_t kTwoSided = 0x80;
constexpr std::uint32_t kFadeBiasMask = 0x1F;
// Its second: four colour offsets, a bin bias, and the draw-mode flag.
constexpr std::uint32_t kBinBiasMask = 0xF8;
constexpr std::uint32_t kDrawMode = 4;

constexpr std::uint32_t kBinBase = 0x40;
constexpr std::uint32_t kFadeBins = 0x20;

constexpr std::uint32_t kG4Tag = 0x08000000u;
constexpr std::uint32_t kG4Code = 0x38000000u;
constexpr std::uint32_t kG4Bytes = 0x24;
constexpr std::uint32_t kG3Tag = 0x06000000u;
constexpr std::uint32_t kG3Code = 0x30000000u;
constexpr std::uint32_t kG3Bytes = 0x1C;
constexpr std::uint32_t kSemiTransparent = 0x02000000u;
constexpr std::uint32_t kDrawModeTag = 0x02000000u;
constexpr std::uint32_t kDrawModeBytes = 0xC;
constexpr std::uint16_t kDrawModeCommand = 0xE100;
constexpr std::uint32_t kDrawModeDither = 0x600;

class FarPass {
public:
  FarPass(TerrainFrame &frame, const Facts &facts)
      : frame_(frame), facts_(facts), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  void drawSector(std::uint32_t entry);
  void drawQuad(std::uint32_t first, std::uint32_t second);
  void drawTriangle(std::uint32_t first, std::uint32_t second);
  bool fadesOut(std::uint32_t depthSum, std::uint32_t first, std::span<std::uint32_t> depths) const;
  void emit(std::uint32_t second, std::uint32_t depthSum, std::span<const std::uint32_t> screens);

  TerrainFrame &frame_;
  const Facts &facts_;
  Core &core_;
  WorkingMemory &memory_;
  std::uint32_t lastBin_ = 0;  // s2
  std::uint32_t handover_ = 0; // s3
  std::array<std::uint32_t, 3> camera_{};
  std::array<std::uint32_t, 3> origin_{}; // s4, s5, s6
  bool edge_ = false;
  std::uint32_t colours_ = 0;
};

void FarPass::run() {
  lastBin_ = memory_.r32(facts_.lodTable + lod::kFarDepth) >> 7;
  handover_ = (memory_.r32(facts_.lodTable + lod::kNearDepth) >> 7) - kFadeBins;
  for (std::uint32_t i = 0; i < camera_.size(); ++i) {
    camera_[i] = sra(frame_.positionWord(i), 4);
  }
  std::uint32_t list = frame_.scratch + kFarList;
  for (;;) {
    const std::uint32_t entry = memory_.r32(list);
    list += 4;
    if (entry == 0) {
      break;
    }
    drawSector(entry);
  }
}

void FarPass::drawSector(std::uint32_t entry) {
  edge_ = (entry & kEdgeTouching) != 0;
  const std::uint32_t sector = entry & ~kEdgeTouching;
  const std::uint32_t originWord = memory_.r32(sector + far_sector::kOrigin);
  origin_ = {(originWord >> 16) - camera_[0],
             camera_[1] - (originWord & 0xFFFFu),
             camera_[2] - memory_.r16(sector + far_sector::kOriginZ)};
  const std::uint32_t layout = memory_.r32(sector + far_sector::kLayout);
  const std::uint32_t data = sector + far_sector::kData;
  const std::uint32_t count = layout & 0xFFu;
  if (!projectFarVertices(frame_,
                          {.data = data, .count = count, .origin = origin_},
                          {.clipped = edge_, .edge = edge_},
                          FarHeight::Plain)) {
    return;
  }
  colours_ = data + 4 * count;
  const std::uint32_t records = colours_ + ((layout >> 6) & 0x3FCu);
  const std::uint32_t end = records + ((layout >> 13) & 0x7F8u);
  for (std::uint32_t record = records; record != end; record += kRecordSize) {
    const std::uint32_t first = memory_.r32(record);
    const std::uint32_t second = memory_.r32(record + 4);
    if (((first >> 11) & 0x1F8u) == ((first >> 5) & 0x1F8u)) {
      drawTriangle(first, second);
    } else {
      drawQuad(first, second);
    }
  }
}

// 800264A4: a polygon nearer than the handover fades out over kFadeBins bins, and is dropped once
// every corner depth is under the fade's start.
bool FarPass::fadesOut(std::uint32_t depthSum,
                       std::uint32_t first,
                       std::span<std::uint32_t> depths) const {
  const std::uint32_t bias = (first & kFadeBiasMask) << 3;
  const std::uint32_t past = bias + depthSum - handover_;
  if (asSigned(past) >= 0) {
    return false;
  }
  if (asSigned(past + kFadeBins) <= 0) {
    return true;
  }
  const std::uint32_t lift = (handover_ - bias) << 3;
  std::uint32_t all = 0xFFFFFFFFu;
  for (std::uint32_t &depth : depths) {
    depth -= lift;
    all &= depth;
  }
  return asSigned(all) < 0;
}

// 80026398.
void FarPass::drawQuad(std::uint32_t first, std::uint32_t second) {
  const std::array<std::uint32_t, 4> cells = {kScratchpad + ((first >> 23) & 0x1F8u),
                                              kScratchpad + ((first >> 17) & 0x1F8u),
                                              kScratchpad + ((first >> 11) & 0x1F8u),
                                              kScratchpad + ((first >> 5) & 0x1F8u)};
  std::array<std::uint32_t, 4> screens{};
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    screens[i] = memory_.r32(cells[i]);
    shared &= screens[i];
  }
  if (edge_) {
    if ((shared & kQuadOutcodes) != 0) {
      return;
    }
    for (std::uint32_t &screen : screens) {
      screen = sra(screen, kFarOutcodeShift);
    }
  }
  core_.gte().writeData(gte::kSxy0, screens[0]);
  core_.gte().writeData(gte::kSxy1, screens[1]);
  core_.gte().writeData(gte::kSxy2, screens[2]);
  core_.gte().execute(gte::kNclip);
  std::array<std::uint32_t, 4> depths{};
  std::uint32_t sum = 0;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    depths[i] = memory_.r32(cells[i] + 4);
    sum += depths[i];
  }
  const std::uint32_t area = core_.gte().readData(gte::kMac0);
  core_.gte().writeData(gte::kSxy0, screens[3]);
  // 80026448: a quad facing away gets a second test on (fourth, second, third).
  if (asSigned(area) <= 0) {
    core_.gte().execute(gte::kNclip);
  }
  sum >>= 5;
  if (asSigned(sum - lastBin_) >= 0) {
    return;
  }
  if (asSigned(area) <= 0 && (first & kTwoSided) == 0 &&
      asSigned(core_.gte().readData(gte::kMac0)) >= 0) {
    return;
  }
  if (fadesOut(sum, first, depths)) {
    return;
  }
  emit(second, sum, screens);
}

// 800265D0.
void FarPass::drawTriangle(std::uint32_t first, std::uint32_t second) {
  const std::array<std::uint32_t, 3> cells = {kScratchpad + ((first >> 23) & 0x1F8u),
                                              kScratchpad + ((first >> 17) & 0x1F8u),
                                              kScratchpad + ((first >> 11) & 0x1F8u)};
  std::array<std::uint32_t, 3> screens{};
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    screens[i] = memory_.r32(cells[i]);
    shared &= screens[i];
  }
  if (edge_) {
    if ((shared & kTriangleOutcodes) != 0) {
      return;
    }
    for (std::uint32_t &screen : screens) {
      screen = sra(screen, kFarOutcodeShift);
    }
  }
  core_.gte().writeData(gte::kSxy0, screens[0]);
  core_.gte().writeData(gte::kSxy1, screens[1]);
  core_.gte().writeData(gte::kSxy2, screens[2]);
  core_.gte().execute(gte::kNclip);
  // 8002663C: two-sided lifts the area past zero.
  const std::uint32_t lift = (first & kTwoSided) << 23;
  if (asSigned(core_.gte().readData(gte::kMac0) + lift) <= 0) {
    return;
  }
  std::array<std::uint32_t, 3> depths{};
  for (std::size_t i = 0; i < cells.size(); ++i) {
    depths[i] = memory_.r32(cells[i] + 4);
  }
  const std::uint32_t sum = (depths[0] + depths[1] + depths[2] + depths[2]) >> 5;
  if (asSigned(sum - lastBin_) >= 0) {
    return;
  }
  if (fadesOut(sum, first, depths)) {
    return;
  }
  emit(second, sum, screens);
}

// 800264D4 / 800266A0: the optional draw-mode packet, then the G4 or G3, each linked into the
// polygon's bin.
void FarPass::emit(std::uint32_t second,
                   std::uint32_t depthSum,
                   std::span<const std::uint32_t> screens) {
  const bool triangle = screens.size() == 3;
  const std::uint32_t bin = depthSum + (second & kBinBiasMask) + kBinBase;
  std::uint32_t code = triangle ? kG3Code : kG4Code;
  if ((second & kDrawMode) != 0) {
    code += kSemiTransparent;
    const std::uint32_t mode = frame_.primitive;
    memory_.w32(mode, kDrawModeTag);
    memory_.w32(mode + 8, 0);
    memory_.w16(mode + 4, static_cast<std::uint16_t>(((second << 5) & 0x7Fu) | kDrawModeDither));
    memory_.w16(mode + 6, kDrawModeCommand);
    frame_.linkAndAdvance(bin, PacketSize{kDrawModeBytes});
  }
  constexpr std::array<std::uint32_t, 4> kColourShifts = {24, 18, 12, 6};
  const std::uint32_t prim = frame_.primitive;
  memory_.w32(prim, triangle ? kG3Tag : kG4Tag);
  for (std::size_t i = 0; i < screens.size(); ++i) {
    std::uint32_t colour = memory_.r32(colours_ + ((second >> kColourShifts[i]) & 0xFCu));
    if (i == 0) {
      colour += code;
    }
    memory_.w32(prim + 4 + 8 * static_cast<std::uint32_t>(i), colour);
    memory_.w32(prim + 8 + 8 * static_cast<std::uint32_t>(i), screens[i]);
  }
  const Linked linked = frame_.linkAndAdvance(bin, PacketSize{triangle ? kG3Bytes : kG4Bytes});
  static_cast<void>(linked);
}

} // namespace

void drawFarSectors(TerrainFrame &frame, const Facts &facts) {
  FarPass(frame, facts).run();
}

} // namespace spyro1::terrain
