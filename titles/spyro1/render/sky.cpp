#include "spyro1/render/sky.h"

#include "core.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"

namespace spyro1::sky {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::signedHalf;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

// g_Cyclorama's words.
constexpr std::uint32_t kSectorCount = 0x0;
constexpr std::uint32_t kSectors = 0x4;
constexpr std::uint32_t kGroupLists = 0xC;

// A sector: its bounding sphere (VXY, then VZ over the radius), its vertex base and count, its
// polygon word, then the packed vertices and their colours.
constexpr std::uint32_t kSphere = 0x00;
constexpr std::uint32_t kBaseXY = 0x08;
constexpr std::uint32_t kBaseZCount = 0x0C;
constexpr std::uint32_t kPolygonWord = 0x10;
constexpr std::uint32_t kVertices = 0x18;

// What the pool must keep free (8004ED40).
constexpr std::uint32_t kPoolReserve = 0x400u;
constexpr std::uint32_t kBinHead = kSkyBin * 8u;

constexpr std::uint32_t kFlatTag = 0x84000000u;
constexpr std::uint32_t kGouraudTag = 0x86000000u;
// POLY_G3's command byte 0x30 less this is POLY_F3's 0x20.
constexpr std::uint32_t kFlatCommand = 0x10000000u;

struct Vertex {
  std::uint32_t z = 0;
  std::uint32_t xy = 0;
};

} // namespace

Cyclorama::Cyclorama(Core &core,
                     const psx::present::EmitMemory &memory,
                     const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds) {}

std::optional<SkyChain> Cyclorama::draw(std::int32_t group, const Rotations &rotations) {
  auto &gte = core_.gte();
  gte.loadRotation(rotations.view);
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  listSectors(group);

  // 8004ECE0..8004ED40
  gte.loadRotation(rotations.projection);
  v0_ = rotations.projection[1];
  v1_ = rotations.projection[2];
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor) + 4u;
  first_ = cursor_;
  last_ = cursor_ - 4u;
  tag_ = 0;
  limit_ = memory_.mem_r32(terrain::kPacketPoolEnd) - kPoolReserve;

  for (std::uint32_t entry = kSectorList;; entry += 4) {
    const std::uint32_t sector = memory_.mem_r32(entry);
    if (sector == 0) {
      break;
    }
    if (!outcodeSector(sector)) {
      continue;
    }
    if (!drawPolygons(memory_.mem_r32(sector + kPolygonWord))) {
      memory_.mem_w32(terrain::kPoolExhausted, 1);
      v0_ = 1;
      break;
    }
  }
  return link();
}

void Cyclorama::end() {
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

// 8004EC14..8004ECDC: every sector, or group `group`'s, whose sphere reaches in front of the eye.
void Cyclorama::listSectors(std::int32_t group) {
  const std::uint32_t sectors = memory_.mem_r32(kCyclorama + kSectors);
  std::uint32_t cursor = kSectorList;
  if (group < 0) {
    const std::uint32_t end = sectors + (memory_.mem_r32(kCyclorama + kSectorCount) << 2);
    for (std::uint32_t at = sectors; at != end; at += 4) {
      keepIfInFront(memory_.mem_r32(at), cursor);
    }
  } else {
    const std::uint32_t lists = memory_.mem_r32(kCyclorama + kGroupLists);
    std::uint32_t bytes = memory_.mem_r32(lists + (static_cast<std::uint32_t>(group) << 2));
    for (;; ++bytes) {
      const std::uint32_t index = memory_.mem_r8(bytes);
      if (index == 0xFFu) {
        break;
      }
      keepIfInFront(memory_.mem_r32(sectors + (index << 2)), cursor);
    }
  }
  memory_.mem_w32(cursor, 0);
}

void Cyclorama::keepIfInFront(std::uint32_t sector, std::uint32_t &cursor) {
  auto &gte = core_.gte();
  const std::uint32_t depth = memory_.mem_r32(sector + kSphere + 4);
  gte.writeData(gte::kVxy0, memory_.mem_r32(sector + kSphere));
  gte.writeData(gte::kVz0, sra(depth, 16));
  gte.execute(gte::kRtps);
  if (asSigned(gte.readData(gte::kMac3) - signedHalf(depth)) > 0) {
    memory_.mem_w32(cursor, sector);
    cursor += 4;
  }
}

// 8004ED50..8004EE58: each vertex projected, its screen word and edges stored in the scratchpad.
bool Cyclorama::outcodeSector(std::uint32_t sector) {
  auto &gte = core_.gte();
  const std::uint32_t base = memory_.mem_r32(sector + kBaseXY);
  const std::uint32_t zCount = memory_.mem_r32(sector + kBaseZCount);
  v0_ = zCount;
  baseY_ = sra(base, 16);
  baseX_ = signedHalf(base);
  baseZ_ = sra(zCount, 16);
  const std::uint32_t vertices = sector + kVertices;
  colours_ = vertices + ((zCount & 0xFFFFu) << 2);
  const auto unpack = [this](std::uint32_t word) {
    return Vertex{.z = (word >> 21) + baseZ_,
                  .xy = (baseX_ - ((word >> 10) & 0x7FFu)) + ((baseY_ - (word & 0x3FFu)) << 16)};
  };

  Vertex next = unpack(memory_.mem_r32(vertices));
  v0_ = next.z;
  v1_ = next.xy;
  gte.writeData(gte::kVz0, next.z);
  gte.writeData(gte::kVxy0, next.xy);
  std::uint32_t edges = 0xFFFFFFFFu;
  std::uint32_t outcode = kOutcodes;
  for (std::uint32_t word = vertices + 4; word != colours_ + 4; word += 4) {
    gte.execute(gte::kRtps);
    next = unpack(memory_.mem_r32(word));
    gte.writeData(gte::kVz0, next.z);
    const std::uint32_t sxy = gte.readData(gte::kSxy2);
    gte.writeData(gte::kVxy0, next.xy);
    v0_ = sxy;
    v1_ = next.xy;
    std::uint32_t code = sxy << 5;
    code += bounds_.aboveTop(sxy) ? 1u : 0u;
    code += bounds_.belowBottom(sxy) ? 2u : 0u;
    code += bounds_.atOrLeftOfLeft(sxy) ? 4u : 0u;
    code += bounds_.atOrRightOfRight(sxy) ? 8u : 0u;
    edges &= code;
    memory_.mem_w32(outcode, code);
    outcode += 4;
  }
  return (edges & 0xFu) == 0;
}

// 8004EE5C..8004EF64: each polygon with a vertex inside every edge, flat when its three colours
// are one.
bool Cyclorama::drawPolygons(std::uint32_t polygons) {
  const std::uint32_t begin = (polygons >> 14) + colours_;
  const std::uint32_t end = ((polygons << 3) & 0xFFF8u) + begin;
  for (std::uint32_t at = begin; at != end; at += 8) {
    if (asSigned(limit_ - cursor_) <= 0) {
      return false;
    }
    const std::uint32_t corners = memory_.mem_r32(at);
    const std::uint32_t a = memory_.mem_r32(kOutcodes + (corners >> 20));
    const std::uint32_t b = memory_.mem_r32(kOutcodes + ((corners >> 10) & 0x3FCu));
    const std::uint32_t c = memory_.mem_r32(kOutcodes + (corners & 0x3FCu));
    v0_ = b;
    v1_ = c;
    const std::uint32_t shades = memory_.mem_r32(at + 4);
    if (asSigned(a & b & c & 0x1Fu) > 0) {
      continue;
    }
    memory_.mem_w32(last_, tag_ ^ cursor_);
    last_ = cursor_;
    const std::uint32_t sxyA = sra(a, 5);
    v0_ = sra(b, 5);
    v1_ = sra(c, 5);
    memory_.mem_w32(cursor_ + 8, sxyA);
    memory_.mem_w32(cursor_ + 0x10, v0_);
    memory_.mem_w32(cursor_ + 0x18, v1_);
    const std::uint32_t shadeA = shades >> 20;
    const std::uint32_t shadeB = (shades >> 10) & 0x3FCu;
    const std::uint32_t shadeC = shades & 0x3FCu;
    if (shadeA == shadeB && shadeA == shadeC) {
      tag_ = kFlatTag;
      memory_.mem_w32(cursor_ + 4, memory_.mem_r32(colours_ + shadeA) - kFlatCommand);
      memory_.mem_w32(cursor_ + 8, sxyA);
      memory_.mem_w32(cursor_ + 0xC, v0_);
      memory_.mem_w32(cursor_ + 0x10, v1_);
      cursor_ += 0x14;
    } else {
      tag_ = kGouraudTag;
      memory_.mem_w32(cursor_ + 4, memory_.mem_r32(colours_ + shadeA));
      memory_.mem_w32(cursor_ + 0xC, memory_.mem_r32(colours_ + shadeB));
      memory_.mem_w32(cursor_ + 0x14, memory_.mem_r32(colours_ + shadeC));
      cursor_ += 0x1C;
    }
  }
  return true;
}

// 8004EF78..8004EFBC: the cursor stored and the chain appended to the deepest bin.
std::optional<SkyChain> Cyclorama::link() {
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
  if (cursor_ == first_) {
    return std::nullopt;
  }
  const std::uint32_t table = memory_.mem_r32(kRenderGlobals.orderingTable);
  tag_ ^= 0x80000000u;
  memory_.mem_w32(last_, tag_);
  v0_ = spyro::depth_bins::linkIntoBin(memory_, table + kBinHead, first_, last_);
  if (v0_ != 0) {
    v1_ = first_ >> 16;
  }
  return SkyChain{.first = first_, .last = last_};
}

} // namespace spyro1::sky
