#include "spyro1/render/lit_actor_emitter.h"

#include "core.h"
#include "core/guest_magnitude.h"
#include "cpu_divide.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/actor_emitter_layout.h"

namespace spyro1::actors {

using spyro::guest_word::absolute;
using spyro::guest_word::signedHalf;

using namespace emitter;
namespace guest_packet = spyro::guest_packet;

namespace {

// 0x8006D908: the angle in 1/512 turns, to 0x40, of a ratio min * 64 / max.
constexpr std::uint32_t kArcTangent = 0x8006D908u;
// 0x8006D808: signed bytes, the reflection's v by the clamped vertical ratio.
constexpr std::uint32_t kArcSine = 0x8006D808u;
// 0x800756F0: the reflection texture's clut and page words.
constexpr std::uint32_t kReflectionTexture = 0x800756F0u;
// The reflection's packed uv once worked out, in place of the vertex's view x.
constexpr std::uint32_t kUvKept = 0x4000u;

constexpr std::uint32_t signedByte(std::uint8_t value) {
  return static_cast<std::uint32_t>(std::int32_t{static_cast<std::int8_t>(value)});
}

// A direction in the plane of two axes.
struct Direction {
  std::uint32_t first = 0;
  std::uint32_t second = 0;
};

// 80022744..80022784, 80022328..80022360: the smaller magnitude over the larger, times 64.
std::uint32_t steepness(const Direction &direction) {
  std::uint32_t low = absolute(direction.first);
  std::uint32_t high = absolute(direction.second);
  if (asSigned(low - high) > 0) {
    const std::uint32_t kept = low;
    low = high;
    high = kept;
  }
  if (high == 0) {
    high = 1;
  }
  return psx::cpu::quotient({low << 6, high});
}

// 80022788..80022858, 800223D8..80022498: the quarter the direction points into, with `entry`
// the arctangent of its steepness, in 1/512 turns.
std::uint32_t turn(const Direction &direction, std::uint32_t entry) {
  const auto [first, second] = direction;
  bool backward = false;
  std::uint32_t base = 0;
  if (asSigned(first) >= 0) {
    if (asSigned(second) >= 0) {
      backward = asSigned(first - second) < 0;
      base = backward ? 0x40u : 0u;
    } else {
      backward = asSigned(first - (0u - second)) >= 0;
      base = backward ? 0x100u : 0xC0u;
    }
  } else if (asSigned(second) >= 0) {
    backward = asSigned((0u - first) - second) >= 0;
    base = backward ? 0x80u : 0x40u;
  } else {
    backward = asSigned((0u - first) - (0u - second)) < 0;
    base = backward ? 0xC0u : 0x80u;
  }
  return backward ? base - entry : base + entry;
}

// 80021EB4..80021F0C, 80022370..800223D0: the length whose square `sum` is, through the
// reciprocal-magnitude table; 0 for 0.
std::uint32_t length(Core &core, std::uint32_t sum, const psx::present::EmitMemory &memory) {
  return spyro::guest_magnitude::magnitude(core.gte(), memory, spyro1::guest::kMagnitudeTable, sum)
      .length();
}

std::uint32_t squaredSum(psx::cpu::Gte &gte) {
  return gte.readData(gte::kMac1) + gte.readData(gte::kMac2) + gte.readData(gte::kMac3);
}

} // namespace

// 800211A0..800211A8: every vertex record goes to the work block.
std::uint32_t LitActorEmitter::depthTable(std::uint32_t /*count*/) const {
  return kWorkDepths;
}

// 800212B4..800212C8, 8002151C..80021540
void LitActorEmitter::keepDepth(const Projected &vertex) {
  depth_ += 8;
  memory_.mem_w16(depth_ - 8, static_cast<std::uint16_t>(vertex.depth));
  memory_.mem_w16(depth_ - 6, static_cast<std::uint16_t>(vertex.view[0]));
  memory_.mem_w16(depth_ - 4, static_cast<std::uint16_t>(vertex.view[1]));
  memory_.mem_w16(depth_ - 2, static_cast<std::uint16_t>(vertex.view[2]));
}

std::uint32_t LitActorEmitter::vertexAt(std::uint32_t slot) const {
  return (slot << 1) + depths_;
}

std::uint32_t LitActorEmitter::depthAt(std::uint32_t slot) const {
  return memory_.mem_r16(vertexAt(slot));
}

// 8002173C..80021774: a secondary word with a light byte aims the reflection first.
void LitActorEmitter::drawPolygons() {
  light_ = memory_.mem_r32(entry_ + entry::kSecondaryWord);
  if ((light_ >> 24) != 0) {
    aimReflection();
  }
  ActorEmitter::drawPolygons();
}

// 800226F8..80022878: light byte 1 keeps the strip reflection, any other the round one (0xFF); the
// light matrix from the light table, and the hue turned by the record's bearing from the camera.
void LitActorEmitter::aimReflection() {
  auto &gte = core_.gte();
  if ((light_ >> 24) != 1) {
    light_ |= 0xFF000000u;
  }
  gte.loadLight(spyro::guest_memory::rotationAt(memory_, spyro1::guest::kMetalMatrix));
  const std::uint32_t z = gte.readControl(gte::kTranslationZ);
  const std::uint32_t x = gte.readControl(gte::kTranslationX);
  const Direction bearing{.first = z, .second = x};
  const std::uint32_t entry = memory_.mem_r8(kArcTangent + steepness(bearing));
  // Retail's lw; only the yaw halfword's bits survive the mask, and an in-between maps just the
  // halfwords.
  const std::uint32_t yaw = memory_.mem_r16(spyro1::guest::kCameraRotationZ);
  light_ += (sra(yaw, 3) - (turn(bearing, entry) << 1)) & 0x1FFu;
}

// 80021884: a two-sided quad is drawn whole, without the second NCLIP.
ActorEmitter::QuadFaces LitActorEmitter::faceQuad(bool twoSided, std::uint32_t fourthScreen) {
  if (twoSided) {
    return QuadFaces::Whole;
  }
  return ActorEmitter::faceQuad(false, fourthScreen);
}

// 80021C0C: a lit triangle takes the flat light with no light byte, else the reflection.
void LitActorEmitter::drawTriangleTail(const Triangle &triangle) {
  if (!triangle.lit) {
    ActorEmitter::drawTriangleTail(triangle);
    return;
  }
  const TriangleColours colours = readColours(triangle);
  if (sra(light_, 24) != 0) {
    drawReflection(triangle, colours);
    return;
  }
  linkTriangle(triangle, flatColours(triangle));
}

// 80021DC4..80021FDC: the face normal from the view positions, its length's reciprocal scaled by
// the light word's top bits, through the light colours: one colour for all three corners.
ActorEmitter::TriangleColours LitActorEmitter::flatColours(const Triangle &triangle) {
  auto &gte = core_.gte();
  std::array<std::uint32_t, 3> x{};
  std::array<std::uint32_t, 3> yz{};
  for (std::size_t i = 0; i < 3; ++i) {
    const std::uint32_t vertex = vertexAt(triangle.slots[i]);
    x[i] = signedHalf(memory_.mem_r16(vertex + 2));
    yz[i] = memory_.mem_r32(vertex + 4);
  }
  gte.writeData(gte::kIr1, x[1] - x[0]);
  gte.writeData(gte::kIr2, sra((yz[1] << 16) - (yz[0] << 16), 16));
  gte.writeData(gte::kIr3, sra(yz[1], 16) - sra(yz[0], 16));
  gte.writeControl(gte::kRotation, x[2] - x[0]);
  gte.writeControl(gte::kRotation + 2, sra((yz[2] << 16) - (yz[0] << 16), 16));
  gte.writeControl(gte::kRotation + 4, sra(yz[2], 16) - sra(yz[0], 16));
  const std::uint32_t backRed = gte.readControl(gte::kBackRed);
  const std::uint32_t backGreen = gte.readControl(gte::kBackGreen);
  gte.execute(psx::gte::kOp);
  const std::array<std::uint32_t, 3> normal = {sra(gte.readData(gte::kMac1), 4),
                                               sra(gte.readData(gte::kMac3), 4),
                                               sra(gte.readData(gte::kMac2), 4)};
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeData(gte::kIr1 + i, normal[i]);
  }
  gte.execute(psx::gte::kSqr);
  gte.writeControl(gte::kBackRed, (light_ >> 6) & 0xFF0u);
  gte.writeControl(gte::kBackGreen, light_ & 0xFF0u);
  gte.writeControl(gte::kBackBlue, (light_ << 6) & 0xFF0u);
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeData(gte::kIr1 + i, normal[i]);
  }
  const std::uint32_t size = length(core_, squaredSum(gte), memory_);
  const std::uint32_t strength = psx::cpu::quotient({(light_ >> 18) << 14, size});
  const std::uint32_t table = spyro1::guest::kSpecularAndMetal;
  const std::uint32_t red = memory_.mem_r32(table + 0xC) & 0xFFFFu;
  const std::uint32_t green = memory_.mem_r32(table + 0x10) & 0xFFFFu;
  const std::uint32_t blue = memory_.mem_r32(table + 0x14) & 0xFFFFu;
  gte.writeControl(psx::gte::kLightColour, (green << 16) + red);
  gte.writeControl(psx::gte::kLightColour + 1, (red << 16) + blue);
  gte.writeControl(psx::gte::kLightColour + 2, (blue << 16) + green);
  gte.writeControl(psx::gte::kLightColour + 3, (green << 16) + red);
  gte.writeControl(psx::gte::kLightColour + 4, blue);
  gte.writeData(gte::kRgbc, 0xFFFFFFu);
  gte.writeData(gte::kIr0, strength);
  gte.execute(gte::kGpf);
  const std::array<std::uint32_t, 3> lit = {sra(gte.readData(gte::kMac1), 8),
                                            sra(gte.readData(gte::kMac2), 8),
                                            sra(gte.readData(gte::kMac3), 8)};
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeData(gte::kIr1 + i, lit[i]);
  }
  gte.execute(gte::kCc);
  const std::uint32_t colour = gte.readData(gte::kRgb2);
  gte.writeControl(gte::kBackRed, backRed);
  gte.writeControl(gte::kBackGreen, backGreen);
  return {.first = colour, .second = colour, .third = colour};
}

// 80021FE0..800222BC: a textured triangle over the reflection texture, its red raised and green
// and blue lowered by the light word's second byte, each corner's uv kept in its vertex record.
void LitActorEmitter::drawReflection(const Triangle &triangle, const TriangleColours &colours) {
  auto &gte = core_.gte();
  const std::uint32_t t8 = bins_.cursor();
  const std::uint32_t tint = (light_ >> 16) & 0xFFu;
  if (tint != 0) {
    const std::array<std::uint32_t, 3> words = {colours.first, colours.second, colours.third};
    for (std::uint32_t i = 0; i < 3; ++i) {
      const std::uint32_t red = (words[i] & 0xFFu) + tint;
      memory_.mem_w8(t8 + 4 + 0xC * i,
                     static_cast<std::uint8_t>(asSigned(red - 0x100u) < 0 ? red : 0xFFu));
    }
    for (std::uint32_t channel = 1; channel < 3; ++channel) {
      for (std::uint32_t i = 0; i < 3; ++i) {
        const std::uint32_t value = ((words[i] >> (8 * channel)) & 0xFFu) - tint;
        memory_.mem_w8(t8 + 4 + 0xC * i + channel,
                       static_cast<std::uint8_t>(asSigned(value) < 0 ? 0u : value));
      }
    }
    memory_.mem_w8(t8 + 7, 0x34);
  } else {
    memory_.mem_w32(t8 + 0x10, colours.second);
    memory_.mem_w32(t8 + 0x1C, colours.third);
    memory_.mem_w32(t8 + 0x04, colours.first + guest_packet::kGt3Code);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kFarRed + i, reflectionUv(vertexAt(triangle.slots[i])));
  }
  const std::uint32_t d0 = depthAt(triangle.slots[0]);
  const std::uint32_t d1 = depthAt(triangle.slots[1]);
  const std::uint32_t d2 = depthAt(triangle.slots[2]);
  std::uint32_t sum = d0 + (d0 >> 1) - depthBase_ + d1 + (d1 >> 1) + d2;
  if (asSigned(sum) < 0) {
    return;
  }
  sum = sra(sum, binShift_ & 31u) + (sra(triangle.colourWord, 28) << 1);
  if (asSigned(sum) < 0) {
    return;
  }
  std::array<std::uint32_t, 3> uv = {gte.readControl(gte::kFarRed),
                                     gte.readControl(gte::kFarGreen),
                                     gte.readControl(gte::kFarBlue)};
  if (asSigned(light_) >= 0) {
    // 800221FC..80022290: a strip that wraps past u 0x80 has its low corners moved up by 0x80.
    std::array<std::uint32_t, 3> u{};
    for (std::uint32_t i = 0; i < 3; ++i) {
      u[i] = (uv[i] & 0xFFu) - 0x40u;
    }
    const auto apart = [](std::uint32_t a, std::uint32_t b) {
      const std::uint32_t d = a - b;
      return asSigned(d - 0x40u) >= 0 || asSigned(d + 0x40u) <= 0;
    };
    const bool mixed = asSigned(u[0] | u[1] | u[2]) < 0 && asSigned(u[0] & u[1] & u[2]) >= 0;
    if (mixed && (apart(u[0], u[1]) || apart(u[0], u[2]) || apart(u[1], u[2]))) {
      for (std::uint32_t i = 0; i < 3; ++i) {
        uv[i] += (u[i] >> 31) << 7;
      }
    }
  }
  const std::uint32_t clut = memory_.mem_r32(kReflectionTexture) + fogClut_;
  const std::uint32_t page = memory_.mem_r32(kReflectionTexture + 4);
  memory_.mem_w32(t8, guest_packet::kGt3Tag);
  memory_.mem_w32(t8 + 0x0C, clut);
  memory_.mem_w32(t8 + 0x18, page);
  memory_.mem_w16(t8 + 0x0C, static_cast<std::uint16_t>(uv[0]));
  memory_.mem_w16(t8 + 0x18, static_cast<std::uint16_t>(uv[1]));
  memory_.mem_w16(t8 + 0x24, static_cast<std::uint16_t>(uv[2]));
  declarePlaces(t8, triangle.slots);
  v1_ = bins_.link((sum << 3) + binsBase_);
}

// 800220E8..8002218C: the vertex's uv, worked out once and kept over its view x and y, z.
std::uint32_t LitActorEmitter::reflectionUv(std::uint32_t vertex) {
  const std::uint32_t x = signedHalf(memory_.mem_r16(vertex + 2));
  const std::uint32_t yz = memory_.mem_r32(vertex + 4);
  if (x == kUvKept) {
    return yz;
  }
  const std::uint32_t uv = directionUv(x, signedHalf(yz), sra(yz, 16));
  memory_.mem_w16(vertex + 2, kUvKept);
  memory_.mem_w32(vertex + 4, uv);
  return uv;
}

// 800222C0..80022568: the vertex's offset from the record's centre through the light matrix; u
// from its bearing and the hue, v from its slope, or both round a circle for the round reflection.
std::uint32_t LitActorEmitter::directionUv(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  auto &gte = core_.gte();
  const std::uint32_t centreX = gte.readControl(gte::kTranslationX);
  const std::array<std::uint32_t, 3> offset = {x - centreX,
                                               y - gte.readControl(gte::kTranslationY),
                                               z - gte.readControl(gte::kTranslationZ)};
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeData(gte::kIr1 + i, offset[i]);
  }
  gte.execute(psx::gte::kMvmvaLlmIr);
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeData(gte::kIr1 + i, offset[i]);
  }
  const std::uint32_t across = gte.readData(gte::kMac1);
  const std::uint32_t along = gte.readData(gte::kMac2);
  const std::uint32_t up = gte.readData(gte::kMac3);
  gte.execute(psx::gte::kSqr);
  const Direction flat{.first = across, .second = along};
  const std::uint32_t ratio = steepness(flat);
  const std::uint32_t sum = squaredSum(gte);
  // A zero offset reads the arctangent at the centre's x, as retail does.
  const std::uint32_t index = sum == 0 ? centreX : ratio;
  const std::uint32_t size = length(core_, sum, memory_);
  const std::uint32_t slope = psx::cpu::quotient({up, size});
  const std::uint32_t bearing = turn(flat, memory_.mem_r8(kArcTangent + index));
  std::uint32_t pitch = sra(slope, 5) + 0x80u;
  if (asSigned(pitch) < 0) {
    pitch = 0;
  } else if (asSigned(pitch - 0x100u) >= 0) {
    pitch = 0xFF;
  }
  const std::uint32_t positionShift = gte.readControl(gte::kBackRed) >> 8;
  std::uint32_t v = signedByte(memory_.mem_r8(kArcSine + pitch));
  std::uint32_t u = (gte.readControl(gte::kTranslationZ) << (positionShift & 31u)) >> 6;
  u = (u + (light_ & 0x1FFu) + bearing) & 0xFFu;
  if (asSigned(light_) >= 0) {
    return (v << 8) | (u >> 1);
  }
  if (asSigned(v - 0x3Fu) > 0) {
    v = 0x3F;
  }
  const std::uint32_t sine = spyro1::guest::kSinTable + (u << 1);
  gte.writeData(gte::kIr0, v);
  gte.writeData(gte::kIr1,
                signedHalf(memory_.mem_r16(sine + spyro::guest_render_globals::kCosineOffset)));
  gte.writeData(gte::kIr2, signedHalf(memory_.mem_r16(sine)));
  gte.execute(gte::kGpf);
  const std::uint32_t right = sra(gte.readData(gte::kMac1), 12) + 0x40u;
  const std::uint32_t down = sra(gte.readData(gte::kMac2), 12) + 0x40u;
  return (down << 8) | right;
}

} // namespace spyro1::actors
