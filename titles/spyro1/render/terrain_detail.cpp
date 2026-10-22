// SCUS_942.28 80026788..80027688: the detail pass. Each near sector's vertices are projected, then
// its polygons are drawn as GT4 / GT3 (G4 / G3 when untextured), each vertex colour faded from its
// near toward its far colour by depth. A polygon whose depths sum under 0x2000 is deferred to the
// coarse or fine split list instead, and every polygon leaves a flag byte its neighbours' split
// passes read: 1 drawn whole, 2 deferred coarse, 4 deferred fine.

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_fog.h"
#include "render/terrain/guest_terrain_mesh.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "spyro1/render/terrain_split.h"

#include <algorithm>
#include <array>
#include <span>

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
namespace guest_packet = spyro::guest_packet;
namespace polygon = spyro::guest_terrain::polygon;
namespace packet = spyro::guest_terrain::packet;
using spyro::guest_terrain::fadeColour;
namespace near_sector = spyro::guest_terrain::near_sector;
using psx::present::EmitMemory;
using spyro::guest_terrain::deferToSplitList;
using spyro::guest_terrain::kCoarseHeaderWritten;
using spyro::guest_terrain::kFineHeaderWritten;
using guest_packet::kG3Bytes;
using guest_packet::kG3Tag;
using guest_packet::kG4Bytes;
using guest_packet::kG4Tag;
using spyro::guest_terrain::kScratchpad;
using spyro::guest_terrain::kSectorClassMask;
using spyro::guest_terrain::kSplitTriangle;
using spyro::guest_terrain::kVertexDepths;
using spyro::guest_terrain::PackedIndices;
using spyro::guest_terrain::PacketSize;
using spyro::guest_terrain::ProjectedSector;
using spyro::guest_terrain::SectorProjector;
using spyro::guest_terrain::unclipCorners;

using spyro::guest_word::asSigned;

// 80026788: the camera parked in the back colour registers, the vertex height (w << 2) & 0xFFC,
// and every close vertex nearer than 0x100 a candidate for the 16x re-projection.
class DetailProjector final : public SectorProjector {
public:
  explicit DetailProjector(TerrainFrame &frame) : SectorProjector(frame) {}

private:
  [[nodiscard]] std::uint32_t height(std::uint32_t word) const override {
    return (word << 2) & 0xFFCu;
  }
  [[nodiscard]] std::uint32_t cameraRegister() const override {
    return gte::kBackRed;
  }
  [[nodiscard]] bool refines(std::uint32_t) const override {
    return true;
  }
  [[nodiscard]] bool reportsOverflow() const override {
    return true;
  }
  [[nodiscard]] bool sumsDepth(bool) const override {
    return false;
  }
  [[nodiscard]] bool testsBehind() const override {
    return false;
  }
};

// The draw word (+0xC); its sides and bin bias are titles/spyro1/render/terrain_split.h's.
constexpr std::uint32_t kFarFade = 1;      // the fade starts 0x2000 further out
constexpr std::uint32_t kNeverFine = 0x40; // deferred coarse when fine, drawn whole when coarse
constexpr std::uint32_t kNeverSplit = 0x80;

constexpr std::uint32_t kSplitSum = 0x2000;
constexpr std::uint32_t kFineVertexDepth = 0x140;
constexpr std::uint32_t kFarFadeShift = 0x2000;
constexpr std::uint32_t kFullBright = 0x1000;
constexpr std::uint32_t kTextureRecordSize = 16;
constexpr std::uint32_t kFullBrightRecord = 8; // the record's second pair: undarkened

// The polygon flag bytes.
constexpr std::uint8_t kDrawnWhole = 1;
constexpr std::uint8_t kDeferredCoarse = 2;
constexpr std::uint8_t kDeferredFine = 4;

constexpr std::uint32_t kUntextured = 0xFFFFFFFFu; // the texture byte, sign-extended
constexpr std::uint32_t kSemiTextured = 0x80;


// 80026F70: the texture record and how many CLUT rows its UV word is darkened by, from how far the
// polygon's mean depth is into the fade; an undarkened polygon reads the record's second pair.
struct TexturePick {
  std::uint32_t record = 0;
  std::uint32_t darken = 0;
};

// A polygon past the culls: its depth sum, its draw word and its split-list entry.
struct Candidate {
  std::uint32_t sum = 0;
  std::uint32_t draw = 0;
  std::uint32_t entry = 0;
};

// One corner's colour index and depth.
struct CornerShade {
  std::uint32_t colourIndex = 0;
  std::uint32_t depth = 0;
};

// The texture byte, sign-extended as `lb` loads it.
std::uint32_t textureIndex(std::uint32_t byte) {
  return (byte & 0x80u) != 0 ? byte | 0xFFFFFF00u : byte;
}

class DetailPass {
public:
  DetailPass(TerrainFrame &frame, const Facts &facts)
      : frame_(frame), facts_(facts), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  void drawSector(std::uint32_t entry);
  void drawQuad(const PackedIndices &vertices);
  void drawTriangle(const PackedIndices &vertices);
  [[nodiscard]] std::uint32_t screenWord(std::uint32_t index) const {
    return memory_.mem_r32(kScratchpad + index);
  }
  [[nodiscard]] std::uint32_t depth(std::uint32_t index) const {
    return memory_.mem_r16(kVertexDepths + (index >> 1));
  }
  // False when the polygon was deferred or dropped instead of drawn.
  bool drawsWhole(std::span<const std::uint32_t> depths, const Candidate &candidate);
  std::uint32_t shade(const CornerShade &corner) const;
  [[nodiscard]] TexturePick pickTexture(const Candidate &candidate, std::uint32_t index) const;

  TerrainFrame &frame_;
  const Facts &facts_;
  Core &core_;
  const EmitMemory &memory_;
  std::uint32_t depthLimit_ = 0; // s5
  std::uint32_t textures_ = 0;   // s3
  std::uint32_t sectorWord_ = 0; // t9: the sector, plus the split headers it has written
  std::uint32_t flagCursor_ = 0; // s6: one past the polygon's flag byte; sign clear = inside
  std::uint32_t nearColours_ = 0;
  std::uint32_t farColours_ = 0;
  std::uint32_t record_ = 0; // the current polygon record
  std::uint32_t fadeBase_ = 0;
};

void DetailPass::run() {
  frame_.fineSplitCursor = frame_.scratch + kFineSplitList;
  frame_.coarseSplitCursor = frame_.scratch + kCoarseSplitList;
  depthLimit_ = memory_.mem_r32(facts_.lodTable + lod::kNearDepth);
  textures_ = memory_.mem_r32(facts_.lodTable + lod::kDetailTextures);
  frame_.v0 = frame_.positionWord(1) >> 2;
  frame_.v1 = frame_.positionWord(2) >> 2;
  core_.gte().writeControl(gte::kBackRed, frame_.positionWord(0) >> 2);
  core_.gte().writeControl(gte::kBackGreen, frame_.v0);
  core_.gte().writeControl(gte::kBackBlue, frame_.v1);
  std::uint32_t list = frame_.scratch + kNearList;
  for (;;) {
    const std::uint32_t entry = memory_.mem_r32(list);
    list += 4;
    if (entry == 0) {
      break;
    }
    drawSector(entry);
  }
  memory_.mem_w32(frame_.coarseSplitCursor, 0);
  memory_.mem_w32(frame_.fineSplitCursor, 0);
}

// 800267F0.
void DetailPass::drawSector(std::uint32_t entry) {
  const std::uint32_t sectorClass = entry & kSectorClassMask;
  sectorWord_ = entry ^ sectorClass;
  const std::uint32_t sector = sectorWord_;
  flagCursor_ = frame_.scratch + (memory_.mem_r32(sector + near_sector::kOriginY) & 0xFFFFu);
  const ProjectedSector projected = DetailProjector(frame_).project(entry);
  if (!projected.drawn) {
    return;
  }
  if (!projected.clipped) {
    flagCursor_ = (flagCursor_ << 1) >> 1;
  }
  // 80026B48: near colours, far colours, then the records.
  const std::uint32_t layout = memory_.mem_r32(sector + near_sector::kLayout);
  const std::uint32_t colourBytes = (layout >> 6) & 0x3FCu;
  nearColours_ = projected.vertexEnd - 8;
  farColours_ = nearColours_ + colourBytes;
  const std::uint32_t records = farColours_ + colourBytes;
  const std::uint32_t end = records + ((layout >> 12) & 0xFF0u);
  for (record_ = records; record_ != end; record_ += polygon::kSize) {
    const PackedIndices vertices(memory_.mem_r32(record_ + polygon::kIndices));
    frame_.v0 = vertices.second;
    frame_.v1 = vertices.third;
    if (vertices.triangle()) {
      drawTriangle(vertices);
    } else {
      drawQuad(vertices);
    }
  }
}

// 80026CFC: drop a polygon past the depth limit, defer a near one to a split list, or say it is
// drawn whole. `entry` is its split-list entry.
bool DetailPass::drawsWhole(std::span<const std::uint32_t> depths, const Candidate &candidate) {
  const auto [sum, draw, entry] = candidate;
  if (asSigned(depthLimit_ - sum) <= 0) {
    return false;
  }
  if (asSigned(sum - kSplitSum) >= 0 || (draw & kNeverSplit) != 0) {
    return true;
  }
  const bool fine = std::any_of(depths.begin(), depths.end(), [](std::uint32_t d) {
    return asSigned(d - kFineVertexDepth) < 0;
  });
  if (fine && sum == 0) {
    return false;
  }
  if (!fine && (draw & kNeverFine) != 0) {
    return true;
  }
  const std::uint32_t header = sectorWord_ >> 2;
  if (fine && (draw & kNeverFine) == 0) {
    deferToSplitList(memory_,
                     {.cursor = frame_.fineSplitCursor, .headerBit = kFineHeaderWritten},
                     sectorWord_,
                     {.header = header, .entry = entry});
    memory_.mem_w8(flagCursor_ - 1, kDeferredFine);
  } else {
    deferToSplitList(memory_,
                     {.cursor = frame_.coarseSplitCursor, .headerBit = kCoarseHeaderWritten},
                     sectorWord_,
                     {.header = header, .entry = entry});
    memory_.mem_w8(flagCursor_ - 1, kDeferredCoarse);
  }
  return false;
}

// 80026DE0: a vertex at least 0x1000 inside the fade is its near colour, one past it its far
// colour, and one between is faded.
std::uint32_t DetailPass::shade(const CornerShade &corner) const {
  const auto [colourIndex, vertexDepth] = corner;
  const std::uint32_t nearColour = memory_.mem_r32(nearColours_ + colourIndex);
  const std::uint32_t fade = fadeBase_ - vertexDepth;
  if (asSigned(fade - kFullBright) >= 0) {
    return nearColour;
  }
  const std::uint32_t farColour = memory_.mem_r32(farColours_ + colourIndex);
  if (asSigned(fade) <= 0) {
    return farColour;
  }
  return fadeColour(core_, {.nearColour = nearColour, .farColour = farColour, .fade = fade});
}

// 80026F70.
TexturePick DetailPass::pickTexture(const Candidate &candidate, std::uint32_t index) const {
  const std::uint32_t past = ((candidate.sum >> 7) << 5) - fadeBase_ + kFullBright;
  TexturePick pick;
  std::uint32_t base = textures_;
  if (asSigned(past) > 0) {
    pick.darken = past >> 8;
  } else {
    base += kFullBrightRecord;
  }
  pick.record = base + (index & 0x7Fu) * kTextureRecordSize;
  return pick;
}

// 80026BA4.
void DetailPass::drawQuad(const PackedIndices &vertices) {
  const bool clipped = asSigned(flagCursor_) <= 0;
  flagCursor_ += 1;
  const auto corners = unclipCorners<4>({screenWord(vertices.first),
                                         screenWord(vertices.second),
                                         screenWord(vertices.third),
                                         screenWord(vertices.fourth)},
                                        clipped,
                                        true);
  if (!corners) {
    return;
  }
  const auto [xy, overflow] = *corners;
  const std::uint32_t draw = memory_.mem_r32(record_ + polygon::kDraw);
  const std::array<std::uint32_t, 4> depths = {
      depth(vertices.first), depth(vertices.second), depth(vertices.third), depth(vertices.fourth)};
  frame_.v0 = depths[1];
  frame_.v1 = depths[2];
  if (!nearQuadFacesCamera(frame_, {xy[0], xy[1], xy[3], xy[2]}, sidedness(draw), overflow != 0)) {
    return;
  }
  const std::uint32_t sum = depths[0] + depths[1] + depths[2] + depths[3];
  const Candidate candidate{.sum = sum, .draw = draw, .entry = record_ + (overflow >> 4)};
  if (!drawsWhole(depths, candidate)) {
    return;
  }

  // 80026DB8: one GT4 (or G4), xy and colours in (first, second, fourth, third) order.
  const std::uint32_t prim = frame_.primitive;
  const std::uint32_t bin = spyro::guest_terrain::polygonBin(sum, binBias(draw));
  memory_.storeVertex(prim + packet::kXy0, memory_.loadVertex(kScratchpad + vertices.first, xy[0]));
  memory_.storeVertex(prim + packet::kXy1, memory_.loadVertex(kScratchpad + vertices.second, xy[1]));
  memory_.storeVertex(prim + packet::kXy2, memory_.loadVertex(kScratchpad + vertices.fourth, xy[3]));
  memory_.storeVertex(prim + packet::kXy3, memory_.loadVertex(kScratchpad + vertices.third, xy[2]));
  fadeBase_ = (depthLimit_ >> 2) + ((draw & kFarFade) != 0 ? kFarFadeShift : 0u);
  const PackedIndices colours(memory_.mem_r32(record_ + polygon::kColours));
  memory_.mem_w32(prim + packet::kColour0,
              shade({.colourIndex = colours.first, .depth = depths[0]}) |
                  guest_packet::kGt4Code);
  memory_.mem_w32(prim + packet::kColour1, shade({.colourIndex = colours.second, .depth = depths[1]}));
  memory_.mem_w32(prim + packet::kColour3, shade({.colourIndex = colours.third, .depth = depths[2]}));
  memory_.mem_w32(prim + packet::kColour2, shade({.colourIndex = colours.fourth, .depth = depths[3]}));
  memory_.mem_w8(flagCursor_ - 1, kDrawnWhole);

  const std::uint32_t texture = textureIndex(memory_.mem_r8(record_ + polygon::kLinks));
  if ((texture & kSemiTextured) != 0) {
    memory_.mem_w8(prim + packet::kCode, psx::gpu::Gp0Command(guest_packet::kSemiGt4Code).opcodeByte());
  }
  if (texture == kUntextured) {
    // 800270F8: the GT4's colour and xy words packed down into a G4.
    const std::uint32_t xy1 = memory_.mem_r32(prim + packet::kXy1);
    const std::uint32_t xy2 = memory_.mem_r32(prim + packet::kXy2);
    const std::uint32_t xy3 = memory_.mem_r32(prim + packet::kXy3);
    const std::uint32_t c1 = memory_.mem_r32(prim + packet::kColour1);
    const std::uint32_t c2 = memory_.mem_r32(prim + packet::kColour2);
    const std::uint32_t c3 = memory_.mem_r32(prim + packet::kColour3);
    memory_.mem_w32(prim + 0x10, xy1);
    memory_.mem_w32(prim + 0x18, xy2);
    memory_.mem_w32(prim + 0x20, xy3);
    memory_.mem_w32(prim + 0x0C, c1);
    memory_.mem_w32(prim + 0x14, c2);
    memory_.mem_w32(prim + 0x1C, c3);
    memory_.mem_w8(prim + packet::kCode, psx::gpu::kPolyG4);
    memory_.mem_w32(prim, guest_packet::kG4Tag);
    frame_.v0 = prim >> 16;
    frame_.v1 = xy3;
    frame_.linkAndAdvance(bin, PacketSize{guest_packet::kG4Bytes});
    return;
  }
  const TexturePick pick = pickTexture(candidate, texture);
  const std::uint32_t uv = memory_.mem_r32(pick.record) + (pick.darken << 22);
  const std::uint32_t page = memory_.mem_r32(pick.record + 4);
  memory_.mem_w32(prim + packet::kUv0, uv);
  memory_.mem_w32(prim + packet::kUv1, page);
  memory_.mem_w32(prim + packet::kUv2, uv + 0x1F00u);
  memory_.mem_w32(prim + packet::kUv3, uv + 0x1F1Fu);
  memory_.mem_w32(prim, guest_packet::kGt4Tag);
  frame_.v0 = prim >> 16;
  frame_.v1 = uv + 0x1F00u;
  frame_.linkAndAdvance(bin, PacketSize{guest_packet::kGt4Bytes});
}

// 80027168.
void DetailPass::drawTriangle(const PackedIndices &vertices) {
  const bool clipped = asSigned(flagCursor_) <= 0;
  flagCursor_ += 1;
  const auto corners = unclipCorners<3>(
      {screenWord(vertices.first), screenWord(vertices.second), screenWord(vertices.third)},
      clipped,
      true);
  if (!corners) {
    return;
  }
  const auto [xy, overflow] = *corners;
  const std::uint32_t draw = memory_.mem_r32(record_ + polygon::kDraw);
  const std::array<std::uint32_t, 3> depths = {
      depth(vertices.first), depth(vertices.second), depth(vertices.third)};
  if (!nearTriangleFacesCamera(frame_, xy, sidedness(draw), overflow != 0)) {
    frame_.v0 = depths[1];
    frame_.v1 = kVertexDepths + (vertices.third >> 1);
    return;
  }
  frame_.v0 = depths[1];
  frame_.v1 = depths[2];
  const std::uint32_t sum = depths[0] + depths[1] + 2 * depths[2];
  const Candidate candidate{
      .sum = sum, .draw = draw, .entry = (record_ + (overflow >> 4)) | kSplitTriangle};
  if (!drawsWhole(depths, candidate)) {
    return;
  }

  // 80027310: one GT3 (or G3).
  const std::uint32_t prim = frame_.primitive;
  const std::uint32_t bin = spyro::guest_terrain::polygonBin(sum, binBias(draw));
  memory_.storeVertex(prim + packet::kXy0, memory_.loadVertex(kScratchpad + vertices.first, xy[0]));
  memory_.storeVertex(prim + packet::kXy1, memory_.loadVertex(kScratchpad + vertices.second, xy[1]));
  memory_.storeVertex(prim + packet::kXy2, memory_.loadVertex(kScratchpad + vertices.third, xy[2]));
  fadeBase_ = (depthLimit_ >> 2) + ((draw & kFarFade) != 0 ? kFarFadeShift : 0u);
  const PackedIndices colours(memory_.mem_r32(record_ + polygon::kColours));
  memory_.mem_w32(prim + packet::kColour0,
              shade({.colourIndex = colours.first, .depth = depths[0]}) |
                  guest_packet::kGt3Code);
  memory_.mem_w32(prim + packet::kColour1, shade({.colourIndex = colours.second, .depth = depths[1]}));
  memory_.mem_w32(prim + packet::kColour2, shade({.colourIndex = colours.third, .depth = depths[2]}));
  memory_.mem_w8(flagCursor_ - 1, kDrawnWhole);

  const std::uint32_t textureWord = memory_.mem_r32(record_ + polygon::kLinks);
  const std::uint32_t texture = textureIndex(textureWord & 0xFFu);
  if ((texture & kSemiTextured) != 0) {
    memory_.mem_w8(prim + packet::kCode, psx::gpu::Gp0Command(guest_packet::kSemiGt3Code).opcodeByte());
  }
  if (texture == kUntextured) {
    // 80027628: packed down into a G3.
    const std::uint32_t xy1 = memory_.mem_r32(prim + packet::kXy1);
    const std::uint32_t xy2 = memory_.mem_r32(prim + packet::kXy2);
    const std::uint32_t c1 = memory_.mem_r32(prim + packet::kColour1);
    const std::uint32_t c2 = memory_.mem_r32(prim + packet::kColour2);
    memory_.mem_w32(prim + 0x10, xy1);
    memory_.mem_w32(prim + 0x18, xy2);
    memory_.mem_w32(prim + 0x0C, c1);
    memory_.mem_w32(prim + 0x14, c2);
    memory_.mem_w8(prim + packet::kCode, psx::gpu::kPolyG3);
    memory_.mem_w32(prim, guest_packet::kG3Tag);
    frame_.v0 = prim >> 16;
    frame_.v1 = c1;
    frame_.linkAndAdvance(bin, PacketSize{guest_packet::kG3Bytes});
    return;
  }
  const TexturePick pick = pickTexture(candidate, texture);
  const std::uint32_t uv = memory_.mem_r32(pick.record) + (pick.darken << 22);
  const std::uint32_t page = memory_.mem_r32(pick.record + 4);
  // 80027574: the corner the triangle drops from the record's square.
  std::array<std::uint32_t, 3> uvs{};
  switch (textureWord & 0x300u) {
  case 0x000u:
    uvs = {uv, page, uv + 0x1F00u};
    frame_.v1 = uv + 0x1F00u;
    break;
  case 0x100u:
    uvs = {uv + 0x1Fu, page + 0x1F00u, uv};
    frame_.v1 = uv + 0x1Fu;
    break;
  case 0x200u:
    uvs = {uv + 0x1F1Fu, page + 0x1EE1u, page};
    frame_.v1 = page + 0x1EE1u;
    break;
  default:
    uvs = {uv + 0x1F00u, page - 0x1Fu, uv + 0x1F1Fu};
    frame_.v1 = uv + 0x1F00u;
    break;
  }
  memory_.mem_w32(prim + packet::kUv0, uvs[0]);
  memory_.mem_w32(prim + packet::kUv1, uvs[1]);
  memory_.mem_w32(prim + packet::kUv2, uvs[2]);
  memory_.mem_w32(prim, guest_packet::kGt3Tag);
  frame_.v0 = prim >> 16;
  frame_.linkAndAdvance(bin, PacketSize{guest_packet::kGt3Bytes});
}

} // namespace

void drawDetailSectors(TerrainFrame &frame, const Facts &facts) {
  DetailPass(frame, facts).run();
}

} // namespace spyro1::terrain
