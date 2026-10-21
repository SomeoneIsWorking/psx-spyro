#include "spyro1/render/glows.h"

#include "gp0_command.h"
#include "core.h"
#include "cpu_divide.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "spyro1/render/screen.h"
#include "spyro1/render/terrain_facts.h"

namespace spyro1::glows {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::signedHalf;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

constexpr std::uint32_t kModeTag = 0x02000000u;
constexpr std::uint32_t kAdditiveMode = 0xE1000220u;
constexpr std::uint32_t kTriangleTag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyG3);
constexpr std::uint32_t kTriangleCommand = 0x32000000u;
constexpr std::uint32_t kLinePairTag = 0x83000000u;
constexpr std::uint32_t kLineTag = 0x03000000u;
constexpr std::uint32_t kLineSemi = 0x40000000u;

std::uint32_t magnitude(std::uint32_t value) {
  return asSigned(value) < 0 ? 0u - value : value;
}

std::uint32_t slotOf(std::uint32_t table, std::uint32_t bin) {
  return (bin << 3) + table;
}


} // namespace

GlowFans::GlowFans(Core &core,
                   const psx::present::EmitMemory &memory,
                   const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds) {}

void GlowFans::begin() {
  auto &gte = core_.gte();
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation));
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
    camera_[i] = memory_.mem_r32(kRenderGlobals.cameraPosition + i * 4);
  }
  table_ = memory_.mem_r32(kRenderGlobals.orderingTable);
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor);
}

void GlowFans::end() {
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
  core_.r[2] = camera_[1];
  core_.r[3] = camera_[2];
}

// 800582B8..800582F4: below-top 1, above-bottom 2, right 4, left 8.
std::uint32_t GlowFans::outcode(Point point) const {
  std::uint32_t code = 0;
  code |= asSigned(point.y - 1) <= 0 ? 1u : 0u;
  code |= asSigned(point.y - 0x100) >= 0 ? 2u : 0u;
  code |= asSigned(point.x - static_cast<std::uint32_t>(bounds_.right())) >= 0 ? 4u : 0u;
  code |= asSigned(point.x - static_cast<std::uint32_t>(bounds_.left())) <= 0 ? 8u : 0u;
  return code;
}

void GlowFans::link(std::uint32_t slot, std::uint32_t packet) {
  const std::uint32_t head = memory_.mem_r32(slot);
  memory_.mem_w32(slot, packet);
  spyro::guest_memory::linkAfter(memory_, head, packet);
}

std::optional<std::uint32_t> GlowFans::draw(std::uint32_t entry) {
  auto &gte = core_.gte();
  std::uint32_t count = memory_.mem_r32(entry + emitter::kCount);
  if (count == 0) {
    return std::nullopt;
  }
  const std::uint32_t position = memory_.mem_r32(entry + emitter::kPosition);
  std::uint32_t x = memory_.mem_r32(position) + memory_.mem_r32(entry + emitter::kOffset) - camera_[0];
  std::uint32_t y =
      camera_[1] - (memory_.mem_r32(position + 4) + memory_.mem_r32(entry + emitter::kOffset + 4));
  std::uint32_t z =
      camera_[2] - (memory_.mem_r32(position + 8) + memory_.mem_r32(entry + emitter::kOffset + 8));
  const std::uint32_t reach = magnitude(x) + magnitude(y) + magnitude(z);
  std::uint32_t shift = 0;
  if (asSigned(reach) > 0) {
    shift = reach >> 13;
    if (asSigned(shift - 4) > 0) {
      shift = 4;
    }
    x = sra(x, shift & 31u);
    y = sra(y, shift & 31u);
    z = sra(z, shift & 31u);
  }
  gte.writeData(gte::kVz0, x);
  gte.writeData(gte::kVxy0, (y & 0xFFFFu) | (z << 16));
  gte.execute(gte::kRtps);
  memory_.mem_w32(cursor_, kModeTag);
  memory_.mem_w32(cursor_ + 4, kAdditiveMode);
  memory_.mem_w32(cursor_ + 8, 0);
  const std::uint32_t centre = gte.readData(gte::kSxy2);
  const std::uint32_t depth = gte.readData(gte::kMac3) << (shift & 31u);
  const std::uint32_t centreY = sra(centre, 16);
  const std::uint32_t centreX = signedHalf(centre);
  const std::uint32_t scale = psx::cpu::quotient({memory_.mem_r32(entry + emitter::kSize) << 12, depth});
  std::uint32_t bin = (depth >> 7) + memory_.mem_r32(entry + emitter::kDepthBias);
  if (asSigned(bin) <= 0) {
    return std::nullopt;
  }
  if (asSigned(bin - 0xFF) > 0) {
    bin += 0x40;
  }
  if (asSigned(bin - 0x7FF) > 0) {
    bin = 0x7FF;
  }
  const std::uint32_t slot = slotOf(table_, bin);
  const std::uint32_t centreEdges = outcode({.x = centreX, .y = centreY});
  gte.writeData(gte::kIr0, scale);
  const std::uint32_t head = memory_.mem_r32(slot);
  memory_.mem_w32(slot, cursor_);
  if (head == 0) {
    memory_.mem_w32(slot + 4, cursor_);
  } else {
    spyro::guest_memory::linkAfter(memory_, head, cursor_);
  }
  cursor_ += 0xC;

  // 8005832C..80058464: the first rim point, then a triangle to each next one.
  std::uint32_t rim = memory_.mem_r32(entry + emitter::kRim);
  gte.writeData(gte::kIr1, memory_.mem_r32(rim));
  gte.writeData(gte::kIr2, memory_.mem_r32(rim + 4));
  rim += 8;
  --count;
  gte.execute(gte::kGpfSf);
  const std::uint32_t colour =
      ((memory_.mem_r32(entry + emitter::kColour) << 8) >> 8) | kTriangleCommand;
  std::uint32_t pointX = gte.readData(gte::kMac1) + centreX;
  std::uint32_t pointY = gte.readData(gte::kMac2) + centreY;
  std::uint32_t edges = outcode({.x = pointX, .y = pointY});
  std::uint32_t point = (pointX & 0xFFFFu) | (pointY << 16);
  do {
    gte.writeData(gte::kIr1, memory_.mem_r32(rim));
    gte.writeData(gte::kIr2, memory_.mem_r32(rim + 4));
    memory_.mem_w32(cursor_, kTriangleTag);
    gte.execute(gte::kGpfSf);
    memory_.mem_w32(cursor_ + 8, centre);
    memory_.mem_w32(cursor_ + 4, colour);
    memory_.mem_w32(cursor_ + 0x10, point);
    memory_.mem_w32(cursor_ + 0xC, 0);
    memory_.mem_w32(cursor_ + 0x14, 0);
    pointX = gte.readData(gte::kMac1) + centreX;
    pointY = gte.readData(gte::kMac2) + centreY;
    const std::uint32_t nextEdges = outcode({.x = pointX, .y = pointY});
    const std::uint32_t next = (pointX & 0xFFFFu) | (pointY << 16);
    memory_.mem_w32(cursor_ + 0x18, next);
    if (asSigned(centreEdges & edges & nextEdges) <= 0) {
      link(slot, cursor_);
      cursor_ += 0x1C;
    }
    --count;
    rim += 8;
    point = next;
    edges = nextEdges;
  } while (asSigned(count) > 0);
  return bin;
}

Sparkles::Sparkles(Core &core,
                   const psx::present::EmitMemory &memory,
                   const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds) {}

void Sparkles::begin(std::uint32_t delta) {
  auto &gte = core_.gte();
  delta_ = delta;
  const psx::gte::RotationWords rotation = spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation);
  gte.loadRotation(rotation);
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    gte.writeControl(gte::kLight0 + i, rotation[i]);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  gte.writeControl(gte::kDqb, 0);
  gte.writeControl(gte::kDqa, 0x100);
  v0_ = rotation[1];
  v1_ = rotation[2];
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  table_ = memory_.mem_r32(kRenderGlobals.orderingTable);
  for (std::uint32_t i = 0; i < 3; ++i) {
    camera_[i] = memory_.mem_r32(kRenderGlobals.cameraPosition + i * 4);
  }
}

void Sparkles::retire(std::uint32_t record) {
  if (retiring_) {
    memory_.mem_w8(record + sparkle::kFade, 0);
  }
}

void Sparkles::end() {
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

std::optional<std::uint32_t> Sparkles::draw(std::uint32_t record) {
  auto &gte = core_.gte();
  retiring_ = false;
  const std::uint32_t word = memory_.mem_r32(record + sparkle::kFade);
  const std::uint32_t fade = (word & 0xFFu) - delta_;
  v1_ = fade;
  v0_ = sra(word, 24);
  const std::uint32_t turn = v0_ * delta_;
  if (asSigned(fade) <= 0) {
    memory_.mem_w8(record + sparkle::kFade, 0);
    return std::nullopt;
  }
  memory_.mem_w8(record + sparkle::kFade, static_cast<std::uint8_t>(fade));
  const std::uint32_t full = (word >> 8) & 0xFFu;
  std::uint32_t spent = (full - fade) << 8;
  if (asSigned(full - fade) < 0) {
    spent = 0u - spent;
  }
  const std::uint32_t angle = (((word >> 16) & 0xFFu) + turn) & 0xFFu;
  memory_.mem_w8(record + sparkle::kAngle, static_cast<std::uint8_t>(angle));
  const std::uint32_t worn = psx::cpu::quotient({spent, full});
  v0_ = sra(memory_.mem_r32(record) - camera_[0], 2);
  const std::uint32_t y = sra(camera_[1] - memory_.mem_r32(record + 4), 2);
  const std::uint32_t z = sra(camera_[2] - memory_.mem_r32(record + 8), 2);
  v1_ = (y & 0xFFFFu) | (z << 16);
  gte.writeData(gte::kVxy0, v1_);
  gte.writeData(gte::kVz0, v0_);
  const std::uint32_t sine = kRenderGlobals.cameraSineTable + (angle << 1);
  gte.execute(gte::kRtps);
  const std::uint32_t sizeDepth = memory_.mem_r32(record + sparkle::kSizeDepth);
  v1_ = memory_.mem_r32(record + sparkle::kCommand);
  const std::uint32_t farthest = sizeDepth & 0xFF00u;
  const std::uint32_t size = sizeDepth & 0xFFu;
  const std::uint32_t depth = gte.readData(gte::kSz3);
  const std::uint32_t screen = gte.readData(gte::kSxy2);
  const bool inView = asSigned(farthest - depth) > 0 && asSigned(depth - 0x80) > 0 &&
                      !bounds_.aboveTop(screen) && !bounds_.belowBottom(screen);
  if (!inView || bounds_.atOrLeftOfLeft(screen) || bounds_.atOrRightOfRight(screen)) {
    memory_.mem_w8(record + sparkle::kFade, 0);
    return std::nullopt;
  }
  retiring_ = kRetailBounds.atOrLeftOfLeft(screen) || kRetailBounds.atOrRightOfRight(screen);

  // 800585F8..80058718: the star's arms scaled by what is left of the fade and turned by the
  // angle, projected around the centre.
  const std::uint32_t command = v1_ | kLineSemi;
  memory_.mem_w32(cursor_ + 4, command);
  memory_.mem_w32(cursor_ + 0x14, command);
  const std::uint32_t reach = (0x100u - worn) * size;
  const std::uint32_t sineValue = signedHalf(memory_.mem_r16(sine));
  const std::uint32_t cosineValue = signedHalf(memory_.mem_r16(sine + spyro::guest_render_globals::kCosineOffset));
  gte.writeData(gte::kIr0, reach);
  gte.writeData(gte::kIr2, sineValue);
  gte.writeData(gte::kIr1, cosineValue);
  const std::uint32_t centre[3] = {
      gte.readData(gte::kMac1), gte.readData(gte::kMac2), gte.readData(gte::kMac3)};
  gte.execute(gte::kGpf);
  const std::uint32_t scale = asSigned(centre[2] - 0x1000) < 0 ? 0x1000u : centre[2];
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, centre[i] << 1);
  }
  const std::uint32_t armX = sra(gte.readData(gte::kMac1), 19);
  const std::uint32_t armY = sra(gte.readData(gte::kMac2), 19);
  gte.writeData(gte::kVz0, 0);
  gte.writeData(gte::kVxy0, ((0u - armX) & 0xFFFFu) + ((0u - armY) << 16));
  gte.writeData(gte::kVz1, 0);
  gte.writeData(gte::kVxy1, (armY & 0xFFFFu) + ((0u - armX) << 16));
  gte.writeData(gte::kVz2, 0);
  gte.writeData(gte::kVxy2, ((0u - armY) & 0xFFFFu) + (armX << 16));
  gte.writeControl(gte::kRotation + 1, 0);
  gte.writeControl(gte::kRotation + 3, 0);
  gte.writeControl(gte::kRotation + 0, scale);
  gte.writeControl(gte::kRotation + 2, scale);
  gte.writeControl(gte::kRotation + 4, scale);
  gte.execute(gte::kRtpt);
  const std::uint32_t first = gte.readData(gte::kSxy0);
  gte.writeData(gte::kVxy0, (armY << 16) + (armX & 0xFFFFu));
  gte.writeData(gte::kVz0, 0);
  const std::uint32_t second = gte.readData(gte::kSxy1);
  const std::uint32_t third = gte.readData(gte::kSxy2);
  gte.execute(gte::kRtps);
  memory_.mem_w32(cursor_ + 8, first);
  memory_.mem_w32(cursor_ + 0x18, second);
  memory_.mem_w32(cursor_ + 0x1C, third);
  std::uint32_t bin = (depth >> 5) - 6;
  if (asSigned(bin) < 0) {
    bin = 0;
  }
  memory_.mem_w32(cursor_, kLineTag);
  if (asSigned(bin - 0x100) > 0) {
    bin += 0x46;
  }
  memory_.mem_w32(cursor_ + 0x10, kLinePairTag ^ cursor_);
  const std::uint32_t slot = slotOf(table_, bin);
  memory_.mem_w32(cursor_ + 0xC, gte.readData(gte::kSxy2));

  // 800587A8..800587E8: the camera back from the light matrix.
  psx::gte::RotationWords rotation{};
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    rotation[i] = gte.readControl(gte::kLight0 + i);
  }
  gte.loadRotation(rotation);
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  v1_ = rotation[2];
  const std::uint32_t head = memory_.mem_r32(slot);
  memory_.mem_w32(slot, cursor_);
  v0_ = cursor_ + 0x10;
  if (head == 0) {
    memory_.mem_w32(slot + 4, v0_);
  } else {
    v1_ = v0_ >> 16;
    spyro::guest_memory::linkAfter(memory_, head, v0_);
  }
  cursor_ += 0x20;
  return bin;
}

} // namespace spyro1::glows
