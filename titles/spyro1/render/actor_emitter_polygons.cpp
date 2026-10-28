// EmitActorDrawList's polygon half: each polygon of the faded list as gouraud or textured gouraud
// triangles and quads, or a scaled sprite quad, into the local bins by depth.
#include "spyro1/render/actor_emitter.h"

#include "core.h"
#include "render/field/guest_packet.h"
#include "spyro1/render/actor_emitter_layout.h"
#include "state_producer.h"

namespace spyro1::actors {

using namespace emitter;
namespace guest_packet = spyro::guest_packet;

namespace {

// A polygon word: corner slots at bits 20, 11 and 2 (the fourth in the next word), and flags.
constexpr std::uint32_t kTwoSided = 0x1u;
constexpr std::uint32_t kTextured = 0x2u;
constexpr std::uint32_t kSprite = 0x4u; // a quad's; a triangle's is kLit
constexpr std::uint32_t kLit = 0x4u;
constexpr std::uint32_t kHidden = 0x8u;

constexpr std::uint32_t slot(std::uint32_t word, std::uint32_t bit) {
  return (word >> bit) & 0x7FCu;
}

constexpr std::uint32_t withoutCommand(std::uint32_t colour) {
  return (colour << 8) >> 8;
}

} // namespace

// 8001FF64..8002074C
void ActorEmitter::drawPolygons() {
  const std::uint32_t sign = core_.gte().readControl(gte::kZsf4) >> 31;
  const std::uint32_t count = memory_.mem_r8(frame_ + frame::kVertexCounts + sign);
  beginPolygons(depthTable(count));
  for (;;) {
    const std::uint32_t colourWord = memory_.mem_r32(polygon_ + 4);
    if (polygon_ == polygonEnd_) {
      return;
    }
    if (asSigned(word_) < 0) {
      if ((word_ & kSprite) != 0) {
        drawSprite(colourWord);
      } else {
        drawQuad(colourWord);
      }
    } else {
      drawTriangle(colourWord);
    }
  }
}

// The screen words at the corner slots; nullopt when a straddling polygon's corners share an
// outcode. Unpacks a straddling record's screen words.
std::optional<std::array<psx::gte::ProjectedVertex, 4>>
ActorEmitter::screens(const std::array<std::uint32_t, 4> &slots, std::uint32_t corners) const {
  std::array<psx::gte::ProjectedVertex, 4> words{};
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::uint32_t i = 0; i < corners; ++i) {
    words[i] = memory_.loadVertex(slots[i] + kVertexScreens);
    shared &= words[i].word();
  }
  if (asSigned(binShift_) >= 0) {
    return words;
  }
  if ((shared & 0x1Fu) != 0) {
    return std::nullopt;
  }
  for (std::uint32_t i = 0; i < corners; ++i) {
    words[i] = memory_.loadVertex(slots[i] + kVertexScreens,
                                  psx::gte::ProjectedVertex::whole(sra(words[i].word(), 5)));
  }
  return words;
}

// 80020000..80020300
void ActorEmitter::drawQuad(std::uint32_t colourWord) {
  auto &gte = core_.gte();
  const std::uint32_t word = word_;
  const std::uint32_t t8 = bins_.cursor();
  std::uint32_t fourthWord = memory_.mem_r32(polygon_ + 8);
  const std::array<std::uint32_t, 4> slots = {
      slot(word, 20), slot(word, 11), slot(word, 2), fourthWord & 0x7FCu};
  const bool textured = (word & kTextured) != 0;
  const auto corners = screens(slots, 4);
  if (corners) {
    gte.writeData(gte::kSxy0, (*corners)[0].word());
    gte.writeData(gte::kSxy1, (*corners)[1].word());
    gte.writeData(gte::kSxy2, (*corners)[2].word());
  }
  if (!corners || (word & kHidden) != 0) {
    polygon_ += textured ? 0x18 : 0xC;
    word_ = memory_.mem_r32(polygon_);
    return;
  }
  const auto [a2, a3, t0, t1] = *corners;
  gte.execute(gte::kNclip);
  memory_.storeVertex(t8 + 0x08, a2);
  if (textured) {
    polygon_ += 0xC;
    memory_.storeVertex(t8 + 0x14, a3);
    memory_.storeVertex(t8 + 0x20, t0);
    memory_.storeVertex(t8 + 0x2C, t1);
  } else {
    memory_.storeVertex(t8 + 0x10, a3);
    memory_.storeVertex(t8 + 0x18, t0);
    memory_.storeVertex(t8 + 0x20, t1);
  }
  const QuadFaces faces = faceQuad((word & kTwoSided) != 0, t1.word());
  const std::uint32_t firstColour = slot(colourWord, 17) + colours_;
  word_ = memory_.mem_r32(polygon_ + 0xC);
  polygon_ += 0xC;
  const std::uint32_t fourthColour = ((fourthWord >> 9) & 0x7FCu) + colours_;
  const Triangle shared{.colourWord = colourWord,
                        .slots = {slots[0], slots[1], slots[2]},
                        .firstColour = firstColour,
                        .textured = textured,
                        .backHalf = false};
  if (faces == QuadFaces::None) {
    return;
  }
  if (faces == QuadFaces::Back) {
    // 80020114: only the back half faces; it is drawn from the fourth corner.
    memory_.storeVertex(t8 + 0x08, t1);
    Triangle backHalf = shared;
    backHalf.slots[0] = slots[3];
    backHalf.firstColour = fourthColour;
    backHalf.backHalf = true;
    drawTriangleTail(backHalf);
    return;
  }
  if (faces == QuadFaces::Front) {
    drawTriangleTail(shared);
    return;
  }
  drawWholeQuad({.colourWord = colourWord,
                 .slots = slots,
                 .colours = {firstColour, fourthColour},
                 .textured = textured});
}

// 80020158..800202FC
void ActorEmitter::drawWholeQuad(const Quad &quad) {
  const std::uint32_t t8 = bins_.cursor();
  const std::uint32_t s0 = quad.colourWord;
  std::uint32_t colour = memory_.mem_r32(quad.colours[0]);
  const std::uint32_t second = memory_.mem_r32(slot(s0, 8) + colours_);
  const std::uint32_t third = memory_.mem_r32(((s0 << 1) & 0x7FCu) + colours_);
  std::array<std::uint32_t, 4> depths{};
  for (std::uint32_t i = 0; i < 4; ++i) {
    depths[i] = depthAt(quad.slots[i]);
  }
  const std::uint32_t fourth = memory_.mem_r32(quad.colours[1]);
  colour = withoutCommand(colour);
  const std::uint32_t shift = binShift_ & 31u;
  const std::uint32_t bias = (sra(s0, 28) << 1) << shift;
  std::uint32_t sum = depths[0] - depthBase_ + depths[1] + depths[2];
  if (!quad.textured) {
    memory_.mem_w32(t8 + 0x04, colour + guest_packet::kG4Code);
    memory_.mem_w32(t8 + 0x0C, second);
    memory_.mem_w32(t8 + 0x14, third);
    memory_.mem_w32(t8 + 0x1C, fourth);
    sum += depths[3];
    if (asSigned(sum) < 0) {
      return;
    }
    memory_.mem_w32(t8, guest_packet::kG4Tag);
    sum += bias;
    if (asSigned(sum) < 0) {
      return;
    }
    v1_ = bins_.link((sra(sum, shift) << 3) + binsBase_);
    return;
  }
  sum += depths[3];
  if (asSigned(sum) < 0) {
    return;
  }
  sum += bias;
  if (asSigned(sum) < 0) {
    return;
  }
  const std::uint32_t uv0 = memory_.mem_r32(polygon_ - 0xC);
  const std::uint32_t uv1 = memory_.mem_r32(polygon_ - 8);
  const std::uint32_t uv2 = memory_.mem_r32(polygon_ - 4);
  memory_.mem_w32(t8 + 0x28, fourth);
  memory_.mem_w32(t8 + 0x1C, third);
  memory_.mem_w32(t8 + 0x10, second);
  memory_.mem_w32(t8 + 0x04, colour + guest_packet::kGt4Code + ((s0 & 1u) << 25));
  memory_.mem_w32(t8, guest_packet::kGt4Tag);
  memory_.mem_w32(t8 + 0x0C, uv0 + fogClut_);
  memory_.mem_w32(t8 + 0x18, uv1);
  memory_.mem_w32(t8 + 0x24, uv2);
  memory_.mem_w32(t8 + 0x30, uv2 >> 16);
  v1_ = bins_.link((sra(sum, shift) << 3) + binsBase_);
}

// 800200B8..80020148: the first NCLIP faces the front half, the second, at the fourth corner,
// the back half; a two-sided quad's halves face either way.
ActorEmitter::QuadFaces ActorEmitter::faceQuad(bool twoSided, std::uint32_t fourthScreen) {
  auto &gte = core_.gte();
  std::uint32_t front = gte.readData(gte::kMac0);
  gte.writeData(gte::kSxy0, fourthScreen);
  if (twoSided && asSigned(front) > 0) {
    front = 0u - front;
  }
  gte.execute(gte::kNclip);
  std::uint32_t back = gte.readData(gte::kMac0);
  if (twoSided && asSigned(back) < 0) {
    back = 0u - back;
  }
  if (asSigned(front) >= 0) {
    return asSigned(back) > 0 ? QuadFaces::Back : QuadFaces::None;
  }
  return asSigned(back) > 0 ? QuadFaces::Whole : QuadFaces::Front;
}

// 8002031C..800203F0
void ActorEmitter::drawTriangle(std::uint32_t colourWord) {
  auto &gte = core_.gte();
  const std::uint32_t word = word_;
  const std::uint32_t t8 = bins_.cursor();
  const std::array<std::uint32_t, 4> slots = {slot(word, 20), slot(word, 11), slot(word, 2), 0};
  const bool textured = (word & kTextured) != 0;
  const auto corners = screens(slots, 3);
  if (!corners) {
    polygon_ += textured ? 0x14 : 0x8;
    word_ = memory_.mem_r32(polygon_);
    return;
  }
  const auto [a2, a3, t0, unused] = *corners;
  gte.writeData(gte::kSxy0, a2.word());
  gte.writeData(gte::kSxy1, a3.word());
  gte.writeData(gte::kSxy2, t0.word());
  gte.execute(gte::kNclip);
  memory_.storeVertex(t8 + 0x08, a2);
  if (textured) {
    polygon_ += 0xC;
    memory_.storeVertex(t8 + 0x14, a3);
    memory_.storeVertex(t8 + 0x20, t0);
  } else {
    memory_.storeVertex(t8 + 0x10, a3);
    memory_.storeVertex(t8 + 0x18, t0);
  }
  polygon_ += 8;
  word_ = memory_.mem_r32(polygon_);
  if ((word & kHidden) != 0) {
    return;
  }
  const std::uint32_t facing = gte.readData(gte::kMac0);
  if ((word & kTwoSided) != 0 ? facing == 0 : asSigned(facing) <= 0) {
    return;
  }
  drawTriangleTail({.colourWord = colourWord,
                    .slots = {slots[0], slots[1], slots[2]},
                    .firstColour = slot(colourWord, 17) + colours_,
                    .textured = textured,
                    .backHalf = false,
                    .lit = (word & kLit) != 0});
}

std::uint32_t ActorEmitter::depthAt(std::uint32_t slot) const {
  return memory_.mem_r32(slot + depths_);
}

void ActorEmitter::drawTriangleTail(const Triangle &triangle) {
  linkTriangle(triangle, readColours(triangle));
}

ActorEmitter::TriangleColours ActorEmitter::readColours(const Triangle &triangle) const {
  const std::uint32_t s0 = triangle.colourWord;
  return {.first = withoutCommand(memory_.mem_r32(triangle.firstColour)),
          .second = memory_.mem_r32(slot(s0, 8) + colours_),
          .third = memory_.mem_r32(((s0 << 1) & 0x7FCu) + colours_)};
}

// 800203F4..800205A4
void ActorEmitter::linkTriangle(const Triangle &triangle, const TriangleColours &colours) {
  const std::uint32_t t8 = bins_.cursor();
  const std::uint32_t s0 = triangle.colourWord;
  const std::uint32_t d0 = depthAt(triangle.slots[0]);
  const std::uint32_t d1 = depthAt(triangle.slots[1]);
  const std::uint32_t d2 = depthAt(triangle.slots[2]);
  const std::uint32_t shift = binShift_ & 31u;
  const std::uint32_t bias = sra(s0, 28) << 1;
  std::uint32_t sum = d0 + (d0 >> 1) - depthBase_ + d1 + (d1 >> 1) + d2;
  if (!triangle.textured) {
    memory_.mem_w32(t8 + 0x04, colours.first + guest_packet::kG3Code);
    memory_.mem_w32(t8 + 0x0C, colours.second);
    memory_.mem_w32(t8 + 0x14, colours.third);
    if (asSigned(sum) < 0) {
      return;
    }
    sum = sra(sum, shift) + bias;
    if (asSigned(sum) < 0) {
      return;
    }
    memory_.mem_w32(t8, guest_packet::kG3Tag);
    v1_ = bins_.link((sum << 3) + binsBase_);
    return;
  }
  if (asSigned(sum) < 0) {
    return;
  }
  sum = sra(sum, shift) + bias;
  if (asSigned(sum) < 0) {
    return;
  }
  const std::uint32_t uv0 = memory_.mem_r32(polygon_ - 0xC);
  const std::uint32_t uv1 = memory_.mem_r32(polygon_ - 8);
  const std::uint32_t uv2 = memory_.mem_r32(polygon_ - 4);
  memory_.mem_w32(t8 + 0x1C, colours.third);
  memory_.mem_w32(t8 + 0x10, colours.second);
  memory_.mem_w32(t8 + 0x04, colours.first + guest_packet::kGt3Code + ((s0 & 1u) << 25));
  memory_.mem_w32(t8 + 0x0C, uv0 + fogClut_);
  if (triangle.backHalf) {
    memory_.mem_w16(t8 + 0x0C, static_cast<std::uint16_t>(uv2 >> 16));
  }
  memory_.mem_w32(t8, guest_packet::kGt3Tag);
  memory_.mem_w32(t8 + 0x18, uv1);
  memory_.mem_w32(t8 + 0x24, uv2);
  v1_ = bins_.link((sum << 3) + binsBase_);
}

// 800205C4..80020748: a quad facing the camera around the first corner, sized by the depth cue of
// an RTPS at that corner's depth.
void ActorEmitter::drawSprite(std::uint32_t colourWord) {
  auto &gte = core_.gte();
  const std::uint32_t word = word_;
  const std::uint32_t t8 = bins_.cursor();
  const std::uint32_t corner = slot(word, 20);
  std::uint32_t screen = memory_.mem_r32(corner + kVertexScreens);
  if (asSigned(binShift_) < 0) {
    if ((screen & 0x1Fu) != 0) {
      word_ = memory_.mem_r32(polygon_ + 0x14);
      polygon_ += 0x14;
      return;
    }
    screen = sra(screen, 5);
  }
  const std::uint32_t depth = depthAt(corner);
  const std::uint32_t colourAt = slot(colourWord, 17) + colours_;
  const std::uint32_t width = (colourWord >> 10) & 0x1FFu;
  gte.writeControl(gte::kDqa, 0x100);
  gte.writeControl(gte::kDqb, 0);
  gte.writeControl(gte::kRotation + 3, 0);
  gte.writeControl(gte::kRotation + 4, 0);
  gte.writeControl(gte::kTranslationZ, depth);
  const std::uint32_t height = (colourWord >> 1) & 0x1FFu;
  gte.execute(gte::kRtps);
  std::uint32_t colour = memory_.mem_r32(colourAt);
  std::uint32_t sum = (depth << 2) + (word & kHidden) - depthBase_;
  word_ = memory_.mem_r32(polygon_ + 0x14);
  polygon_ += 0x14;
  if (asSigned(sum) <= 0) {
    return;
  }
  sum = sra(sum, binShift_ & 31u);
  const std::uint32_t bin = (sum << 3) + binsBase_;
  const std::uint32_t cue = gte.readData(gte::kMac0);
  gte.writeData(gte::kIr1, width);
  gte.writeData(gte::kIr2, height);
  gte.writeData(gte::kIr0, cue >> 12);
  colour = withoutCommand(colour);
  gte.execute(gte::kGpf);
  colour += guest_packet::kFt4Code + ((colourWord & 1u) << 25);
  const std::uint32_t sizeX = gte.readData(gte::kMac1) >> 11;
  const std::uint32_t sizeY = gte.readData(gte::kMac2) >> 11;
  const std::uint32_t uv0 = memory_.mem_r32(polygon_ - 0xC);
  const std::uint32_t uv1 = memory_.mem_r32(polygon_ - 8);
  const std::uint32_t uv2 = memory_.mem_r32(polygon_ - 4);
  const std::uint32_t right = (screen & 0xFFFFu) + (sizeX >> 1);
  const std::uint32_t left = right - sizeX;
  memory_.mem_w32(t8 + 0x04, colour);
  memory_.mem_w32(t8 + 0x0C, uv0);
  memory_.mem_w32(t8 + 0x14, uv1);
  memory_.mem_w32(t8 + 0x1C, uv2);
  memory_.mem_w32(t8 + 0x24, uv2 >> 16);
  const std::uint32_t bottom = (screen >> 16) + (sizeY >> 1);
  const std::uint32_t top = bottom - sizeY;
  const auto half = [](std::uint32_t value) {
    return static_cast<std::uint16_t>(value);
  };
  memory_.mem_w16(t8 + 0x08, half(left));
  memory_.mem_w16(t8 + 0x0A, half(top));
  memory_.mem_w16(t8 + 0x10, half(right));
  memory_.mem_w16(t8 + 0x12, half(top));
  memory_.mem_w16(t8 + 0x18, half(left));
  memory_.mem_w16(t8 + 0x1A, half(bottom));
  memory_.mem_w16(t8 + 0x20, half(right));
  memory_.mem_w16(t8 + 0x22, half(bottom));
  memory_.mem_w32(t8, guest_packet::kFt4Tag);
  v1_ = bins_.link(bin);
}

} // namespace spyro1::actors
