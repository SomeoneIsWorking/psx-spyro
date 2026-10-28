#include "spyro1/render/sky.h"

#include "core.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_fog.h"
#include "spyro1/render/terrain_facts.h"
#include "state_producer.h"

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
  gte.loadTranslation({});
  listSectors(kCyclorama, group);

  // 8004ECE0..8004ED40
  beginChain(rotations);
  v0_ = rotations.projection[1];
  v1_ = rotations.projection[2];
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
  return link(memory_.mem_r32(kRenderGlobals.orderingTable) + kBinHead);
}

std::optional<SkyChain> Cyclorama::drawThrough(std::uint32_t sky,
                                               const Rotations &rotations,
                                               std::optional<SkyFade> fade,
                                               std::uint32_t slot,
                                               TriangleCut &cut) {
  auto &gte = core_.gte();
  gte.loadRotation(rotations.view);
  gte.loadTranslation({});
  listSectors(sky, -1);
  beginChain(rotations);
  for (std::uint32_t entry = kSectorList;; entry += 4) {
    const std::uint32_t sector = memory_.mem_r32(entry);
    if (sector == 0) {
      break;
    }
    const std::uint32_t polygons = memory_.mem_r32(sector + kPolygonWord);
    std::uint32_t colours =
        sector + kVertices + ((memory_.mem_r32(sector + kBaseZCount) & 0xFFFFu) << 2);
    // 800503D0..80050468
    if (fade) {
      spyro::guest_terrain::loadFarColour(gte, fade->colour);
      spyro::guest_terrain::fadeColours(
          memory_, core_, colours, colours + (polygons >> 14) + 4, fade->step, kFadedColours);
      colours = kFadedColours;
    }
    if (!outcodeSector(sector)) {
      continue;
    }
    const std::uint32_t begin = (polygons >> 14) + colours_;
    const std::uint32_t end = ((polygons << 3) & 0xFFF8u) + begin;
    for (std::uint32_t at = begin; at != end; at += 8) {
      const Polygon polygon = polygonAt(at);
      if (asSigned(polygon.outcodes[0] & polygon.outcodes[1] & polygon.outcodes[2] & 0x1Fu) > 0) {
        continue;
      }
      cut.cut(triangle(polygon, colours), *this);
    }
    cut.finishSector(*this);
  }
  return link(slot);
}

// 8004ECE0 / 8004F5A4: the projection rotation and an empty chain at the packet cursor.
void Cyclorama::beginChain(const Rotations &rotations) {
  core_.gte().loadRotation(rotations.projection);
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor) + 4u;
  first_ = cursor_;
  last_ = cursor_ - 4u;
  tag_ = 0;
}

void Cyclorama::end() {
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

// 8004EC14..8004ECDC: every sector, or group `group`'s, whose sphere reaches in front of the eye.
void Cyclorama::listSectors(std::uint32_t sky, std::int32_t group) {
  const std::uint32_t sectors = memory_.mem_r32(sky + kSectors);
  std::uint32_t cursor = kSectorList;
  if (group < 0) {
    const std::uint32_t end = sectors + (memory_.mem_r32(sky + kSectorCount) << 2);
    for (std::uint32_t at = sectors; at != end; at += 4) {
      keepIfInFront(memory_.mem_r32(at), cursor);
    }
  } else {
    const std::uint32_t lists = memory_.mem_r32(sky + kGroupLists);
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
    const psx::gte::ProjectedVertex projected = gte.projected(2);
    const std::uint32_t sxy = projected.word();
    gte.writeData(gte::kVxy0, next.xy);
    v0_ = sxy;
    v1_ = next.xy;
    std::uint32_t code = sxy << 5;
    code += bounds_.aboveTop(sxy) ? 1u : 0u;
    code += bounds_.belowBottom(sxy) ? 2u : 0u;
    code += bounds_.atOrLeftOfLeft(sxy) ? 4u : 0u;
    code += bounds_.atOrRightOfRight(sxy) ? 8u : 0u;
    edges &= code;
    memory_.storeVertex(outcode, {code, projected.place()});
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
    const Polygon polygon = polygonAt(at);
    v0_ = polygon.outcodes[1];
    v1_ = polygon.outcodes[2];
    if (asSigned(polygon.outcodes[0] & polygon.outcodes[1] & polygon.outcodes[2] & 0x1Fu) > 0) {
      continue;
    }
    const SkyTriangle drawn = triangle(polygon, colours_);
    v0_ = drawn.points[1].word();
    v1_ = drawn.points[2].word();
    emit(drawn, polygon.shades[0] == polygon.shades[1] && polygon.shades[0] == polygon.shades[2]);
  }
  return true;
}

// The corner and shade bytes at `at` as outcode words and colour offsets.
Cyclorama::Polygon Cyclorama::polygonAt(std::uint32_t at) const {
  const std::uint32_t corners = memory_.mem_r32(at);
  const std::uint32_t shades = memory_.mem_r32(at + 4);
  const std::array<std::uint32_t, 3> offsets{
      corners >> 20, (corners >> 10) & 0x3FCu, corners & 0x3FCu};
  return Polygon{
      .corners = offsets,
      .outcodes = {memory_.mem_r32(kOutcodes + offsets[0]),
                   memory_.mem_r32(kOutcodes + offsets[1]),
                   memory_.mem_r32(kOutcodes + offsets[2])},
      .shades = {shades >> 20, (shades >> 10) & 0x3FCu, shades & 0x3FCu},
  };
}

SkyTriangle Cyclorama::triangle(const Polygon &polygon, std::uint32_t colours) const {
  SkyTriangle out;
  for (std::uint32_t corner = 0; corner < 3; ++corner) {
    out.points[corner] =
        memory_.loadVertex(kOutcodes + polygon.corners[corner],
                           psx::gte::ProjectedVertex::whole(sra(polygon.outcodes[corner], 5)));
    out.colours[corner] = memory_.mem_r32(colours + polygon.shades[corner]);
  }
  return out;
}

// 8004EED4..8004EF64 / 8004FC3C..8004FC98
void Cyclorama::emit(const SkyTriangle &triangle, bool flat) {
  memory_.mem_w32(last_, tag_ ^ cursor_);
  last_ = cursor_;
  const auto &p = triangle.points;
  const auto &c = triangle.colours;
  if (flat) {
    tag_ = kFlatTag;
    memory_.mem_w32(cursor_ + 4, c[0] - kFlatCommand);
    memory_.storeVertex(cursor_ + 8, p[0]);
    memory_.storeVertex(cursor_ + 0xC, p[1]);
    memory_.storeVertex(cursor_ + 0x10, p[2]);
    cursor_ += 0x14;
  } else {
    tag_ = kGouraudTag;
    memory_.mem_w32(cursor_ + 4, c[0]);
    memory_.storeVertex(cursor_ + 8, p[0]);
    memory_.mem_w32(cursor_ + 0xC, c[1]);
    memory_.storeVertex(cursor_ + 0x10, p[1]);
    memory_.mem_w32(cursor_ + 0x14, c[2]);
    memory_.storeVertex(cursor_ + 0x18, p[2]);
    cursor_ += 0x1C;
  }
}

// 8004EF78..8004EFBC / 8004FD10..8004FD6C: the cursor stored and the chain appended to the bin at
// `slot`.
std::optional<SkyChain> Cyclorama::link(std::uint32_t slot) {
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
  if (cursor_ == first_) {
    return std::nullopt;
  }
  tag_ ^= 0x80000000u;
  memory_.mem_w32(last_, tag_);
  v0_ = spyro::depth_bins::linkIntoBin(memory_, slot, first_, last_);
  if (v0_ != 0) {
    v1_ = first_ >> 16;
  }
  return SkyChain{.first = first_, .last = last_};
}

} // namespace spyro1::sky
