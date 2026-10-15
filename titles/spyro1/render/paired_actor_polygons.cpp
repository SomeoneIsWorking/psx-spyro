// RasterizePairedActor's polygon half: the whole and cut passes into the local bins, and the merge
// of those bins into the ordering table.
#include "spyro1/render/paired_actor.h"

#include "core.h"
#include "spyro1/render/paired_actor_layout.h"

namespace spyro1::actors {

using namespace paired;

namespace {

constexpr std::uint32_t kColourMask = 0xFFFFFFu;
constexpr std::uint32_t kGouraudTexturedQuad = 0x3C000000u;
constexpr std::uint32_t kGouraudTexturedTriangle = 0x34000000u;
constexpr std::uint32_t kQuadTag = 0x0C000000u;
constexpr std::uint32_t kTriangleTag = 0x09000000u;

// MIPS DIV's quotient.
std::uint32_t quotient(std::uint32_t dividend, std::uint32_t divisor) {
  const std::int64_t n = asSigned(dividend);
  const std::int64_t d = asSigned(divisor);
  if (d == 0) {
    return n >= 0 ? 0xFFFFFFFFu : 1u;
  }
  return static_cast<std::uint32_t>(n / d);
}

} // namespace

// 80024C30..80024C68: the bins, the depth base and shift parked in the back colour registers, and
// the packet cursor.
void PairedActor::beginPolygons() {
  auto &gte = core_.gte();
  depthBase_ = gte.readControl(gte::kBackBlue) << 2;
  binShift_ = gte.readControl(gte::kBackRed) + 4;
  bins_.begin({.base = kLocalBins, .cursor = memory_.r32(kRenderGlobals.primitiveCursor)});
  polygon_ = memory_.r32(header_ + kPolygonList);
  polygonEnd_ = memory_.r32(polygon_);
  polygon_ += 4;
  polygonEnd_ += polygon_;
}

// The depth sum's bin, or nullopt when the sum is not positive.
std::optional<std::uint32_t> PairedActor::binOf(std::uint32_t sum, std::uint32_t &shifted) const {
  const bool behind = asSigned(sum) <= 0;
  shifted = sum >> (binShift_ & 31u);
  if (behind) {
    return std::nullopt;
  }
  return (shifted << 3) + kLocalBins;
}

// 80024C70..80025044: textured gouraud polygons, one-sided unless flagged; a one-sided quad with
// one half facing is drawn as that half.
void PairedActor::drawWhole() {
  auto &gte = core_.gte();
  const std::uint32_t screens = kVertexScreens;
  std::uint32_t word = memory_.r32(polygon_);
  for (;;) {
    const std::uint32_t s0 = memory_.r32(polygon_ + 4);
    v0_ = word >> 20;
    if (polygon_ == polygonEnd_) {
      return;
    }
    v0_ &= 0x7FCu;
    v1_ = (word >> 11) & 0x7FCu;
    const std::uint32_t a0 = (word >> 2) & 0x7FCu;
    const std::uint32_t twoSided = word & 1u;
    const std::uint32_t a2 = memory_.r32(v0_ + screens);
    const std::uint32_t a3 = memory_.r32(v1_ + screens);
    const std::uint32_t t0 = memory_.r32(a0 + screens);
    gte.writeData(gte::kSxy0, a2);
    gte.writeData(gte::kSxy1, a3);
    gte.writeData(gte::kSxy2, t0);
    gte.execute(gte::kNclip);
    memory_.w32(bins_.cursor() + 0x08, a2);
    memory_.w32(bins_.cursor() + 0x14, a3);
    memory_.w32(bins_.cursor() + 0x20, t0);
    const std::uint32_t t2 = ((s0 >> 17) & 0x7FCu) + colours_;

    if (asSigned(word) >= 0) {
      // 80024EA8: a triangle.
      if (twoSided == 0) {
        const std::uint32_t facing = gte.readData(gte::kMac0);
        word = memory_.r32(polygon_ + 0x14);
        polygon_ += 0x14;
        if (asSigned(facing) <= 0) {
          continue;
        }
      } else {
        word = memory_.r32(polygon_ + 0x14);
        polygon_ += 0x14;
      }
      const std::uint32_t t3 = ((s0 >> 8) & 0x7FCu) + colours_;
      emitWholeTriangle({.colourWord = s0, .third = a0, .colours = {t2, t3, 0}, .end = polygon_},
                        false);
      continue;
    }

    // 80024C9C: a quad.
    std::uint32_t t5 = memory_.r32(polygon_ + 8);
    const std::uint32_t a1 = t5 & 0x7FCu;
    const std::uint32_t t1 = memory_.r32(a1 + screens);
    memory_.w32(bins_.cursor() + 0x2C, t1);
    if (twoSided != 0) {
      t5 = ((t5 >> 9) & 0x7FCu) + colours_;
      word = memory_.r32(polygon_ + 0x18);
      polygon_ += 0x18;
      const std::uint32_t t3 = ((s0 >> 8) & 0x7FCu) + colours_;
      emitWholeQuad(
          {.colourWord = s0, .third = a0, .fourth = a1, .colours = {t2, t3, t5}, .end = polygon_});
      continue;
    }
    const std::uint32_t front = gte.readData(gte::kMac0);
    gte.writeData(gte::kSxy0, t1);
    gte.execute(gte::kNclip);
    word = memory_.r32(polygon_ + 0x18);
    polygon_ += 0x18;
    t5 = ((t5 >> 9) & 0x7FCu) + colours_;
    const std::uint32_t t3 = ((s0 >> 8) & 0x7FCu) + colours_;
    const std::uint32_t back = gte.readData(gte::kMac0);
    if (asSigned(front) >= 0) {
      if (asSigned(back) <= 0) {
        continue;
      }
      memory_.w32(bins_.cursor() + 0x08, t1);
      v0_ = a1;
      emitWholeTriangle({.colourWord = s0, .third = a0, .colours = {t5, t3, 0}, .end = polygon_},
                        true);
    } else if (asSigned(back) > 0) {
      emitWholeQuad(
          {.colourWord = s0, .third = a0, .fourth = a1, .colours = {t2, t3, t5}, .end = polygon_});
    } else {
      emitWholeTriangle({.colourWord = s0, .third = a0, .colours = {t2, t3, 0}, .end = polygon_},
                        false);
    }
  }
}

// 80024D98..80024EA4
void PairedActor::emitWholeQuad(const WholePolygon &polygon) {
  const auto &[s0, a0, a1, colourAddresses, ra] = polygon;
  const auto [t2, t3, t5] = colourAddresses;
  const std::uint32_t depths = kVertexTable;
  const std::uint32_t t4 = memory_.r32(((s0 << 1) & 0x7FCu) + colours_);
  const std::uint32_t colour = (memory_.r32(t2) & kColourMask) + kGouraudTexturedQuad;
  v0_ = memory_.r16((v0_ << 2) + depths);
  v1_ = memory_.r16((v1_ << 2) + depths);
  const std::uint32_t c = memory_.r16((a0 << 2) + depths);
  const std::uint32_t d = memory_.r16((a1 << 2) + depths);
  const std::uint32_t bias = (sra(s0, 28) << 2) << (binShift_ & 31u);
  const std::uint32_t sum = v0_ - depthBase_ + v1_ + c + bias + d;
  const std::optional<std::uint32_t> bin = binOf(sum, v0_);
  if (!bin) {
    return;
  }
  v0_ = *bin;
  const std::uint32_t uv = memory_.r32(ra - 4);
  memory_.w32(bins_.cursor() + 0x10, memory_.r32(t3));
  memory_.w32(bins_.cursor() + 0x1C, t4);
  memory_.w32(bins_.cursor() + 0x28, memory_.r32(t5));
  memory_.w32(bins_.cursor() + 0x04, colour + ((s0 & 1u) << 25));
  memory_.w32(bins_.cursor(), kQuadTag);
  memory_.w32(bins_.cursor() + 0x0C, memory_.r32(ra - 0xC));
  memory_.w32(bins_.cursor() + 0x18, memory_.r32(ra - 8));
  memory_.w32(bins_.cursor() + 0x24, uv);
  memory_.w32(bins_.cursor() + 0x30, uv >> 16);
  v1_ = bins_.link(v0_);
}

// 80024F3C..80025044. `s1` positive: the quad's back half, whose first uv halfword is its fourth's.
void PairedActor::emitWholeTriangle(const WholePolygon &polygon, bool backHalf) {
  const std::uint32_t s0 = polygon.colourWord;
  const std::uint32_t a0 = polygon.third;
  const std::uint32_t t2 = polygon.colours[0];
  const std::uint32_t t3 = polygon.colours[1];
  const std::uint32_t ra = polygon.end;
  const std::uint32_t depths = kVertexTable;
  const std::uint32_t t4 = memory_.r32(((s0 << 1) & 0x7FCu) + colours_);
  const std::uint32_t colour = (memory_.r32(t2) & kColourMask) + kGouraudTexturedTriangle;
  v0_ = memory_.r16((v0_ << 2) + depths);
  v1_ = memory_.r16((v1_ << 2) + depths);
  const std::uint32_t c = memory_.r16((a0 << 2) + depths);
  const std::uint32_t bias = (sra(s0, 28) << 2) << (binShift_ & 31u);
  std::uint32_t sum = v0_ + (v0_ >> 1) - depthBase_ + v1_;
  v1_ >>= 1;
  sum += v1_ + c + bias;
  const std::optional<std::uint32_t> bin = binOf(sum, v0_);
  if (!bin) {
    return;
  }
  v0_ = *bin;
  const std::uint32_t uv = memory_.r32(ra - 4);
  memory_.w32(bins_.cursor() + 0x10, memory_.r32(t3));
  memory_.w32(bins_.cursor() + 0x1C, t4);
  memory_.w32(bins_.cursor() + 0x04, colour + ((s0 & 1u) << 25));
  memory_.w32(bins_.cursor() + 0x0C, memory_.r32(ra - 0xC));
  if (backHalf) {
    memory_.w16(bins_.cursor() + 0x0C, static_cast<std::uint16_t>(uv >> 16));
  }
  memory_.w32(bins_.cursor(), kTriangleTag);
  memory_.w32(bins_.cursor() + 0x18, memory_.r32(ra - 8));
  memory_.w32(bins_.cursor() + 0x24, uv);
  v1_ = bins_.link(v0_);
}

// 80025048..8002579C: every polygon as triangles, each dropped when all three corners are under
// water and cut along its crossing edges when some are.
void PairedActor::drawCut() {
  for (;;) {
    std::uint32_t s1 = memory_.r32(polygon_);
    if (polygon_ == polygonEnd_) {
      return;
    }
    const std::uint32_t s2 = memory_.r32(polygon_ + 4);
    std::array<std::uint32_t, 3> corners = {(s1 >> 18) & 0xFF0u, (s1 >> 9) & 0xFF0u, s1 & 0xFF0u};
    v0_ = corners[1];
    v1_ = corners[2];
    std::array<std::uint32_t, 3> colours = {
        (s2 >> 17) & 0x7FCu, (s2 >> 8) & 0x7FCu, (s2 << 1) & 0x7FCu};
    const std::uint32_t words = asSigned(s1) < 0 ? 0xC : 8;
    std::array<std::uint32_t, 3> uvs = {memory_.r32(polygon_ + words),
                                        memory_.r32(polygon_ + words + 4),
                                        memory_.r32(polygon_ + words + 8)};
    polygon_ += words + 0xC;
    for (;;) {
      cutTriangle(
          {.word = s1, .colourWord = s2, .corners = corners, .colourOffsets = colours, .uvs = uvs});
      if (asSigned(s1) >= 0) {
        break;
      }
      // 800250B8: a quad's second half takes its fourth corner and colour in place of the first.
      const std::uint32_t fourth = memory_.r32(polygon_ - 0x10);
      corners = {(fourth << 2) & 0xFF0u, (s1 >> 9) & 0xFF0u, s1 & 0xFF0u};
      v0_ = corners[1];
      v1_ = corners[2];
      colours = {(fourth >> 9) & 0x7FCu, (s2 >> 8) & 0x7FCu, (s2 << 1) & 0x7FCu};
      uvs[0] = (uvs[0] & 0xFFFF0000u) | (uvs[2] >> 16);
      s1 = (s1 << 1) >> 1;
    }
  }
}

// 80025140..8002579C
void PairedActor::cutTriangle(const CutTriangle &triangle) {
  const auto &[s1, s2, corners, colourOffsets, uvs] = triangle;
  auto &gte = core_.gte();
  const std::uint32_t depths = kVertexTable;
  std::array<std::uint32_t, 3> heights{};
  for (std::uint32_t i = 0; i < 3; ++i) {
    heights[i] = memory_.r32(corners[i] + depths + vertex_entry::kVxy);
  }
  if (asSigned(heights[0] & heights[1] & heights[2]) < 0) {
    return;
  }
  std::uint32_t depth = memory_.r32(corners[0] + depths);
  const std::uint32_t b = memory_.r32(corners[1] + depths);
  const std::uint32_t c = memory_.r32(corners[2] + depths);
  std::array<std::uint32_t, 3> screens{};
  for (std::uint32_t i = 0; i < 3; ++i) {
    screens[i] = memory_.r32((corners[i] >> 2) + kVertexScreens);
  }
  v0_ = screens[1];
  v1_ = screens[2];
  gte.writeData(gte::kSxy0, screens[0]);
  gte.writeData(gte::kSxy1, screens[1]);
  gte.writeData(gte::kSxy2, screens[2]);
  memory_.w32(bins_.cursor() + 0x08, screens[0]);
  memory_.w32(bins_.cursor() + 0x14, screens[1]);
  gte.execute(gte::kNclip);
  memory_.w32(bins_.cursor() + 0x20, screens[2]);
  depth += depth >> 1;
  depth += b >> 1;
  if ((s1 & 1u) == 0) {
    std::uint32_t facing = gte.readData(gte::kMac0);
    if (asSigned(s1) < 0) {
      facing = 0u - facing;
    }
    if (asSigned(facing) <= 0) {
      return;
    }
  }
  std::array<std::uint32_t, 3> colours{};
  for (std::uint32_t i = 0; i < 3; ++i) {
    colours[i] = memory_.r32(colourOffsets[i] + colours_);
  }
  const std::uint32_t bias = (sra(s2, 28) << 2) << (binShift_ & 31u);
  std::uint32_t shifted = 0;
  if (asSigned(heights[0] | heights[1] | heights[2]) >= 0) {
    const std::optional<std::uint32_t> bin = binOf(depth - depthBase_ + b + c + bias, shifted);
    if (!bin) {
      return;
    }
    memory_.w32(bins_.cursor() + 0x04, ((colours[0] << 8) >> 8) + kGouraudTexturedTriangle);
    memory_.w32(bins_.cursor() + 0x10, colours[1]);
    memory_.w32(bins_.cursor() + 0x1C, colours[2]);
    memory_.w32(bins_.cursor() + 0x0C, uvs[0]);
    memory_.w32(bins_.cursor() + 0x18, uvs[1]);
    memory_.w32(bins_.cursor() + 0x24, uvs[2]);
    memory_.w32(bins_.cursor(), kTriangleTag);
    bins_.link(*bin);
    return;
  }

  // 800252C0: the corners above water and the crossings between, three or four of them.
  memory_.w32(bins_.cursor() + 0x0C, uvs[0]);
  memory_.w32(bins_.cursor() + 0x18, uvs[1]);
  const std::optional<std::uint32_t> bin = binOf(depth - depthBase_ + b + c + bias, shifted);
  if (!bin) {
    return;
  }
  written_ = bins_.cursor();
  for (std::uint32_t i = 0; i < 3; ++i) {
    const std::uint32_t next = (i + 1) % 3;
    if (asSigned(heights[i]) >= 0) {
      memory_.w32(written_ + 4, colours[i]);
      memory_.w32(written_ + 8, screens[i]);
      memory_.w16(written_ + 0xC, static_cast<std::uint16_t>(uvs[i]));
      written_ += 0xC;
    }
    if (asSigned(heights[i] ^ heights[next]) < 0) {
      cutEdge({.heights = {heights[i], heights[next]},
               .from = {colours[i], screens[i], uvs[i]},
               .to = {colours[next], screens[next], uvs[next]}});
    }
  }
  if (written_ - bins_.cursor() - 0x30 != 0) {
    memory_.w8(bins_.cursor() + 7, static_cast<std::uint8_t>(kGouraudTexturedTriangle >> 24));
    memory_.w32(bins_.cursor(), kTriangleTag);
    bins_.link(*bin);
    return;
  }
  // 80025710: four corners, the last two swapped into quad order.
  std::array<std::uint32_t, 6> tail{};
  for (std::uint32_t i = 0; i < 6; ++i) {
    tail[i] = memory_.r32(bins_.cursor() + 0x1C + 4 * i);
  }
  v0_ = tail[1];
  v1_ = tail[2];
  for (std::uint32_t i = 0; i < 6; ++i) {
    memory_.w32(bins_.cursor() + 0x1C + 4 * i, tail[(i + 3) % 6]);
  }
  memory_.w8(bins_.cursor() + 7, static_cast<std::uint8_t>(kGouraudTexturedQuad >> 24));
  memory_.w32(bins_.cursor(), kQuadTag);
  bins_.link(*bin);
}

// 8002531C..8002542C: the point where an edge crosses the surface, its colour, screen position and
// uv interpolated by INTPL at height from / (from - to).
void PairedActor::cutEdge(const CutEdge &edge) {
  auto &gte = core_.gte();
  const auto [from, to] = edge.heights;
  const std::uint32_t weight = quotient(from << 12, from - to);
  // Field 0 is the colour's three bytes; the screen word and uv take two channels.
  const auto channels = [&gte, &edge](std::uint32_t field, auto split) {
    const std::uint32_t count = field == 0 ? 3 : 2;
    for (std::uint32_t i = 0; i < count; ++i) {
      gte.writeData(gte::kIr1 + i, split(edge.from[field], i));
      gte.writeControl(gte::kFarRed + i, split(edge.to[field], i));
    }
  };
  const auto byte = [](std::uint32_t word, std::uint32_t i) {
    return (word >> (8 * i)) & 0xFFu;
  };
  const auto half = [](std::uint32_t word, std::uint32_t i) {
    return sra(word << (16 - 16 * i), 16);
  };
  channels(0, byte);
  gte.writeData(gte::kIr0, weight);
  gte.execute(gte::kIntpl);
  std::uint32_t colour = (gte.readData(gte::kMac3) << 8) + gte.readData(gte::kMac2);
  colour = (colour << 8) + gte.readData(gte::kMac1);
  memory_.w32(written_ + 4, colour);
  channels(1, half);
  gte.execute(gte::kIntpl);
  memory_.w32(written_ + 8,
              (gte.readData(gte::kMac1) & 0xFFFFu) + (gte.readData(gte::kMac2) << 16));
  channels(2, byte);
  gte.execute(gte::kIntpl);
  memory_.w16(
      written_ + 0xC,
      static_cast<std::uint16_t>(gte.readData(gte::kMac1) + (gte.readData(gte::kMac2) << 8)));
  written_ += 0xC;
}

// 800257A0..800258AC: the local bins, nearest first, into the ordering table's bins from the
// centre's depth on, several local bins to one.
bool PairedActor::mergeBins() {
  if (bins_.empty()) {
    return false;
  }
  auto &gte = core_.gte();
  const LocalBins::Merged merged =
      bins_.merge({.shift = gte.readControl(gte::kBackRed),
                   .depthBin = gte.readControl(gte::kBackGreen),
                   .table = memory_.r32(kRenderGlobals.orderingTable)});
  v0_ = merged.table;
  v1_ = merged.stride;
  memory_.w32(kRenderGlobals.primitiveCursor, bins_.cursor());
  return true;
}

} // namespace spyro1::actors
