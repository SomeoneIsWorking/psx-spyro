#include "spyro1/render/particles.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "spyro1/render/screen.h"
#include "spyro1/render/terrain_facts.h"

namespace spyro1::particles {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

constexpr std::uint32_t kSineTable = kRenderGlobals.cameraSineTable;
constexpr std::uint32_t kCosineOffset = 0x80u;
constexpr std::uint32_t kTop = 0x00010000u;    // $s4
constexpr std::uint32_t kBottom = 0x01000000u; // $s5
constexpr std::uint32_t kPoolMargin = 0x400u;
constexpr std::uint32_t kQuadTag = 0x09000000u;
constexpr std::uint32_t kLineTag = 0x04000000u;
constexpr std::uint32_t kDotTag = 0x03000000u;

std::uint32_t signedHalf(std::uint32_t word) {
  return sra(word << 16, 16);
}

bool retailDrops(std::uint32_t sxy) {
  return kRetailBounds.atOrLeftOfLeft(sxy) || kRetailBounds.atOrRightOfRight(sxy);
}

std::uint32_t sineAt(spyro::guest_memory::WorkingMemory &memory, std::uint32_t entry) {
  return static_cast<std::uint32_t>(static_cast<std::int16_t>(memory.r16(entry)));
}

} // namespace

EmitList::EmitList(Core &core,
                   spyro::guest_memory::WorkingMemory &memory,
                   const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), leftEdge_(static_cast<std::uint32_t>(bounds.left()) << 16),
      rightEdge_(static_cast<std::uint32_t>(bounds.right()) << 16) {}

void EmitList::begin() {
  auto &gte = core_.gte();
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    gte.writeControl(gte::kRotation + i, memory_.r32(kRenderGlobals.cameraRotation + i * 4));
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    camera_[i] = sra(memory_.r32(kRenderGlobals.cameraPosition + i * 4), 2);
  }
  gte.writeControl(gte::kDqb, 0);
  gte.writeControl(gte::kDqa, 0x100);
  limit_ = memory_.r32(terrain::kPacketPoolEnd) - kPoolMargin;
  cursor_ = memory_.r32(kRenderGlobals.primitiveCursor);
  table_ = memory_.r32(kRenderGlobals.orderingTable);
  v0_ = core_.r[2];
  v1_ = core_.r[3];
}

void EmitList::end(bool poolFull) {
  if (poolFull) {
    v0_ = 1;
    memory_.w32(kPoolExhausted, v0_);
  }
  memory_.w32(kRenderGlobals.primitiveCursor, cursor_);
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

void EmitList::markSeen(std::uint32_t at, bool retailDropped) {
  memory_.w8(at + record::kSeen, retailDropped ? 0 : 1);
}

bool EmitList::leftOf(std::uint32_t sxy) const {
  return asSigned((sxy << 16) - leftEdge_) <= 0;
}

bool EmitList::rightOf(std::uint32_t sxy) const {
  return asSigned((sxy << 16) - rightEdge_) >= 0;
}

std::uint32_t EmitList::textureGroup() const {
  return memory_.r32(((kind_ & 0xFFu) << 2) + kTextureGroups);
}

void EmitList::writeUvs(std::uint32_t first, std::uint32_t second) {
  memory_.w32(cursor_ + 0x0C, first);
  memory_.w32(cursor_ + 0x24, second);
  memory_.w32(cursor_ + 0x1C, (first & 0xFFu) | (second & 0xFF00u));
  memory_.w32(cursor_ + 0x14, (second ^ (second & 0xFF00u)) | (first & 0xFF00u));
}

std::uint32_t EmitList::link(std::uint32_t binOffset, Packet packet) {
  const std::uint32_t slot = binOffset + table_;
  const std::uint32_t head = memory_.r32(slot);
  memory_.w32(slot, cursor_);
  v0_ = cursor_ >> 16;
  if (head == 0) {
    memory_.w32(slot + 4, cursor_);
  } else {
    memory_.w16(head, static_cast<std::uint16_t>(cursor_));
    memory_.w8(head + 2, static_cast<std::uint8_t>(v0_));
  }
  cursor_ += static_cast<std::uint32_t>(packet);
  return slot;
}

Outcome EmitList::draw(std::uint32_t at) {
  kind_ = signedHalf(memory_.r16(at + record::kKind));
  if (asSigned(kind_) < 0) {
    return sra(kind_, 8) + 1 == 0 ? Outcome::End : Outcome::Skipped;
  }
  switch (sra(kind_, 8)) {
  case 0:
    return dot(at);
  case 1:
    return line(at);
  case 2:
    return turnedQuad(at);
  case 3:
    return scaledQuad(at);
  case 4:
    return placedLine(at);
  case 5:
    return placedTurnedQuad(at);
  default:
    return quad(at);
  }
}

// 800574F8: a quad turned by the record's angle about the view axis, sized by its scale byte.
Outcome EmitList::quad(std::uint32_t at) {
  auto &gte = core_.gte();
  std::uint32_t x = memory_.r32(at + 4);
  std::uint32_t a0 = memory_.r32(at + 8);
  if (asSigned(limit_ - cursor_) <= 0) {
    return Outcome::PoolFull;
  }
  std::uint32_t v0 = x >> 16;
  x = (x & 0xFFFFu) - camera_[0];
  memory_.w8(at + record::kSeen, 0);
  std::uint32_t v1 = a0 & 0xFFFFu;
  const std::uint32_t entry = ((a0 >> 23) & 0x1FEu) + kSineTable;
  const std::uint32_t sine = sineAt(memory_, entry);
  const std::uint32_t cosine = sineAt(memory_, entry + kCosineOffset);
  a0 = (a0 >> 16) & 0xFFu;
  v0 = camera_[1] - v0;
  v1 = (camera_[2] - v1) << 16;
  gte.writeData(gte::kIr0, a0);
  gte.writeData(gte::kIr1, sine);
  gte.writeData(gte::kIr2, cosine);
  gte.execute(gte::kGpf);
  const std::uint32_t frames = textureGroup();
  const std::uint32_t a1 = sra(gte.readData(gte::kMac1), 12);
  a0 = sra(gte.readData(gte::kMac2), 12);
  gte.writeData(gte::kVxy0, v0 - a1 + v1);
  gte.writeData(gte::kVz0, x + a0);
  gte.writeData(gte::kVxy1, v0 + a0 + v1);
  gte.writeData(gte::kVz1, x + a1);
  gte.writeData(gte::kVxy2, v0 - a0 + v1);
  gte.writeData(gte::kVz2, x - a1);
  const std::uint32_t lastZ = x - a0;
  const std::uint32_t lastXy = v0 + a1 + v1;
  gte.execute(gte::kRtpt);
  const std::uint32_t frameBias = memory_.r16(at + 0x10);
  const std::uint32_t colour = memory_.r32(at + 0xC);
  const std::uint32_t bias = frameBias >> 8;
  const std::uint32_t frame = ((frameBias & 0xFFu) << 3) + frames;
  const std::uint32_t first = memory_.r32(frame + 4);
  const std::uint32_t second = memory_.r32(frame + 8);
  const std::uint32_t s0 = gte.readData(gte::kSxy0);
  v0_ = gte.readData(gte::kSxy1);
  v1_ = gte.readData(gte::kSxy2);
  gte.writeData(gte::kVxy0, lastXy);
  gte.writeData(gte::kVz0, lastZ);
  std::uint32_t depth = gte.readData(gte::kSz1) + gte.readData(gte::kSz2) + gte.readData(gte::kSz3);
  gte.execute(gte::kRtps);
  memory_.w32(cursor_ + 4, colour);
  memory_.w32(cursor_, kQuadTag);
  writeUvs(first, second);
  const std::uint32_t s3 = gte.readData(gte::kSxy2);
  depth += gte.readData(gte::kSz3);
  if (asSigned(depth - 0x200) < 0 || asSigned(depth - 0x8000) >= 0) {
    return Outcome::Skipped;
  }
  const std::uint32_t bin = (depth >> 7) - bias;
  if (asSigned(bin) < 0) {
    return Outcome::Skipped;
  }
  memory_.w32(cursor_ + 0x08, s0);
  memory_.w32(cursor_ + 0x10, v0_);
  memory_.w32(cursor_ + 0x18, v1_);
  memory_.w32(cursor_ + 0x20, s3);
  const std::uint32_t corners[4] = {s0, v0_, v1_, s3};
  const auto all = [&](auto outside) {
    for (const std::uint32_t corner : corners) {
      if (!outside(corner)) {
        return false;
      }
    }
    return true;
  };
  if (all([](std::uint32_t sxy) {
        return asSigned(sxy - kTop) <= 0;
      }) ||
      all([](std::uint32_t sxy) {
        return asSigned(sxy - kBottom) >= 0;
      }) ||
      all([this](std::uint32_t sxy) {
        return leftOf(sxy);
      }) ||
      all([this](std::uint32_t sxy) {
        return rightOf(sxy);
      })) {
    return Outcome::Skipped;
  }
  markSeen(at,
           all([](std::uint32_t sxy) {
             return kRetailBounds.atOrLeftOfLeft(sxy);
           }) ||
               all([](std::uint32_t sxy) {
                 return kRetailBounds.atOrRightOfRight(sxy);
               }));
  link(bin << 3, Packet::Quad);
  return Outcome::Drawn;
}

// 80057750: a turned quad whose record holds its view-space centre / 4; no view test.
Outcome EmitList::placedTurnedQuad(std::uint32_t at) {
  auto &gte = core_.gte();
  const std::uint32_t a1 = memory_.r32(at + 4);
  const std::uint32_t a2 = memory_.r32(at + 8);
  if (asSigned(limit_ - cursor_) <= 0) {
    return Outcome::PoolFull;
  }
  const std::uint32_t frames = textureGroup();
  const std::uint32_t scale = (a2 >> 16) & 0xFFu;
  const std::uint32_t entry = (((a2 >> 23) + 0x40u) & 0x1FEu) + kSineTable;
  gte.writeData(gte::kIr0, scale);
  gte.writeData(gte::kIr1, sineAt(memory_, entry));
  gte.writeData(gte::kIr2, sineAt(memory_, entry + kCosineOffset));
  const std::uint32_t x = sra(a1 << 16, 14);
  gte.execute(gte::kGpf);
  const std::uint32_t y = sra(a1, 14);
  const std::uint32_t z = sra(a2 << 16, 14);
  gte.writeControl(gte::kTranslationX, x);
  gte.writeControl(gte::kTranslationY, y);
  gte.writeControl(gte::kTranslationZ, z);
  return turnedTail(at, {.frames = frames, .depth = z >> 7});
}

// 80057954: a line between two view-space points / 4, tested at its first.
Outcome EmitList::placedLine(std::uint32_t at) {
  auto &gte = core_.gte();
  const std::uint32_t first = memory_.r32(at + 4);
  const std::uint32_t a0 = memory_.r32(at + 8);
  v1_ = signedHalf(a0);
  gte.writeControl(gte::kTranslationX, signedHalf(first));
  gte.writeControl(gte::kTranslationY, sra(first, 16));
  gte.writeControl(gte::kTranslationZ, v1_);
  gte.writeData(gte::kVxy0, 0);
  gte.writeData(gte::kVz0, 0);
  if (asSigned(limit_ - cursor_) <= 0) {
    return Outcome::PoolFull;
  }
  gte.execute(gte::kRtps);
  const std::uint32_t second = memory_.r32(at + 0xC);
  memory_.w8(at + record::kSeen, 0);
  gte.writeControl(gte::kTranslationX, sra(a0, 16));
  gte.writeControl(gte::kTranslationY, signedHalf(second));
  gte.writeControl(gte::kTranslationZ, sra(second, 16));
  v0_ = gte.readData(gte::kSz3);
  const std::uint32_t sxy = gte.readData(gte::kSxy2);
  gte.execute(gte::kRtps);
  const std::uint32_t colour = memory_.r32(at + 0x10);
  std::uint32_t colourBias = memory_.r32(at + 0x14);
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  const std::uint32_t depth = v0_;
  v1_ = depth - 0x40;
  if (asSigned(v1_) <= 0) {
    v1_ = depth - 0x800;
    return Outcome::Skipped;
  }
  v1_ = depth - 0x800;
  if (asSigned(v1_) >= 0) {
    v1_ = sxy - kTop;
    return Outcome::Skipped;
  }
  v1_ = sxy - kTop;
  if (asSigned(v1_) <= 0) {
    v1_ = sxy - kBottom;
    return Outcome::Skipped;
  }
  v1_ = sxy - kBottom;
  if (asSigned(v1_) >= 0) {
    v1_ = sxy << 16;
    return Outcome::Skipped;
  }
  v1_ = (sxy << 16) - rightEdge_;
  if (leftOf(sxy)) {
    return Outcome::Skipped;
  }
  v0_ = depth >> 5;
  if (rightOf(sxy)) {
    return Outcome::Skipped;
  }
  markSeen(at, retailDrops(sxy));
  v1_ = gte.readData(gte::kSxy2);
  memory_.w32(cursor_ + 0x08, sxy);
  memory_.w32(cursor_ + 0x10, v1_);
  memory_.w32(cursor_ + 0x04, colour);
  memory_.w32(cursor_ + 0x0C, colourBias);
  colourBias >>= 24;
  v0_ -= colourBias;
  if (asSigned(v0_) < 0) {
    v0_ = 0;
  }
  memory_.w32(cursor_, kLineTag);
  v0_ <<= 3;
  v1_ = link(v0_, Packet::Line);
  return Outcome::Drawn;
}

// 80057A74: an unturned quad around a camera-relative point, its sides by two size bytes.
Outcome EmitList::scaledQuad(std::uint32_t at) {
  auto &gte = core_.gte();
  std::uint32_t x = memory_.r32(at + 4);
  const std::uint32_t a0 = memory_.r32(at + 8);
  if (asSigned(limit_ - cursor_) <= 0) {
    return Outcome::PoolFull;
  }
  std::uint32_t v0 = x >> 16;
  x = (x & 0xFFFFu) - camera_[0];
  v0 = camera_[1] - v0;
  gte.writeData(gte::kVz0, x);
  std::uint32_t v1 = (camera_[2] - (a0 & 0xFFFFu)) << 16;
  gte.writeData(gte::kVxy0, v0 + v1);
  memory_.w8(at + record::kSeen, 0);
  gte.execute(gte::kRtps);
  std::uint32_t frames = textureGroup();
  gte.writeData(gte::kIr1, (a0 >> 16) & 0xFFu);
  gte.writeData(gte::kIr2, a0 >> 24);
  v1 = gte.readData(gte::kSz3);
  v0 = gte.readData(gte::kSxy2);
  v0_ = v0;
  v1_ = v1;
  if (asSigned(v1 - 0x80) < 0 || asSigned(v1 - 0x2000) >= 0 || asSigned(v0 - kTop) <= 0) {
    return Outcome::Skipped;
  }
  const std::uint32_t frameBias = memory_.r16(at + 0x10);
  if (asSigned(v0 - kBottom) >= 0 || leftOf(v0)) {
    return Outcome::Skipped;
  }
  v1 >>= 5;
  v1_ = v1;
  if (rightOf(v0)) {
    return Outcome::Skipped;
  }
  markSeen(at, retailDrops(v0));
  gte.writeData(gte::kIr0, gte.readData(gte::kMac0) >> 12);
  v1 -= frameBias >> 8;
  v1_ = v1;
  if (asSigned(v1) < 0) {
    return Outcome::Skipped;
  }
  gte.execute(gte::kGpf);
  frames += (frameBias & 0xFFu) << 3;
  const std::uint32_t first = memory_.r32(frames + 4);
  const std::uint32_t second = memory_.r32(frames + 8);
  const std::uint32_t width = gte.readData(gte::kMac1) >> 12;
  const std::uint32_t height = gte.readData(gte::kMac2) >> 12;
  const std::uint32_t colour = memory_.r32(at + 0xC);
  v0 += (width >> 1) + ((height >> 1) << 16);
  memory_.w32(cursor_ + 0x04, colour);
  memory_.w32(cursor_ + 0x20, v0);
  memory_.w32(cursor_ + 0x18, v0 - width);
  memory_.w32(cursor_ + 0x10, v0 - (height << 16));
  memory_.w32(cursor_ + 0x08, v0 - (height << 16) - width);
  writeUvs(first, second);
  memory_.w32(cursor_, kQuadTag);
  v1_ = link(v1 << 3, Packet::Quad);
  return Outcome::Drawn;
}

// 80057C08: a turned quad around a camera-relative point.
Outcome EmitList::turnedQuad(std::uint32_t at) {
  auto &gte = core_.gte();
  std::uint32_t x = memory_.r32(at + 4);
  const std::uint32_t a0 = memory_.r32(at + 8);
  if (asSigned(limit_ - cursor_) <= 0) {
    return Outcome::PoolFull;
  }
  std::uint32_t v0 = x >> 16;
  x = (x & 0xFFFFu) - camera_[0];
  v0 = camera_[1] - v0;
  gte.writeData(gte::kVz0, x);
  v0 += (camera_[2] - (a0 & 0xFFFFu)) << 16;
  memory_.w8(at + record::kSeen, 0);
  gte.writeData(gte::kVxy0, v0);
  gte.execute(gte::kRtps);
  const std::uint32_t frames = textureGroup();
  v0_ = (a0 >> 16) & 0xFFu;
  const std::uint32_t entry = (((a0 >> 23) + 0x40u) & 0x1FEu) + kSineTable;
  std::uint32_t depth = gte.readData(gte::kSz3);
  const std::uint32_t sxy = gte.readData(gte::kSxy2);
  v1_ = sineAt(memory_, entry);
  if (asSigned(depth - 0x80) < 0 || asSigned(depth - 0x2000) >= 0 || asSigned(sxy - kTop) <= 0) {
    return Outcome::Skipped;
  }
  const std::uint32_t cosine = sineAt(memory_, entry + kCosineOffset);
  if (asSigned(sxy - kBottom) >= 0 || leftOf(sxy) || rightOf(sxy)) {
    return Outcome::Skipped;
  }
  depth >>= 5;
  gte.writeData(gte::kIr0, v0_);
  gte.writeData(gte::kIr1, v1_);
  gte.writeData(gte::kIr2, cosine);
  const std::uint32_t centreX = gte.readData(gte::kMac1) << 2;
  const std::uint32_t centreY = gte.readData(gte::kMac2) << 2;
  const std::uint32_t centreZ = gte.readData(gte::kMac3) << 2;
  gte.execute(gte::kGpf);
  markSeen(at, retailDrops(sxy));
  gte.writeControl(gte::kTranslationX, centreX);
  gte.writeControl(gte::kTranslationY, centreY);
  gte.writeControl(gte::kTranslationZ, centreZ);
  return turnedTail(at, {.frames = frames, .depth = depth});
}

Outcome EmitList::turnedTail(std::uint32_t at, TurnedQuad quad) {
  std::uint32_t depth = quad.depth;
  auto &gte = core_.gte();
  const std::uint32_t a2 = sra(gte.readData(gte::kMac1), 10);
  const std::uint32_t a3 = sra(gte.readData(gte::kMac2), 10);
  gte.writeData(gte::kVz0, 0);
  gte.writeData(gte::kVxy0, ((0u - a2) & 0xFFFFu) + ((0u - a3) << 16));
  gte.writeData(gte::kVz1, 0);
  gte.writeData(gte::kVxy1, (a3 & 0xFFFFu) + ((0u - a2) << 16));
  gte.writeData(gte::kVz2, 0);
  gte.writeData(gte::kVxy2, ((0u - a3) & 0xFFFFu) + (a2 << 16));
  std::uint32_t rotation[gte::kRotationWords];
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    rotation[i] = gte.readControl(gte::kRotation + i);
  }
  // Facing the camera: the rotation a diagonal scale, y by 0xA00 / 0x1000.
  gte.writeControl(gte::kRotation + 1, 0);
  gte.writeControl(gte::kRotation + 3, 0);
  gte.writeControl(gte::kRotation + 0, 0x1000);
  gte.writeControl(gte::kRotation + 4, 0x1000);
  gte.writeControl(gte::kRotation + 2, 0xA00);
  gte.execute(gte::kRtpt);
  v0_ = (a2 & 0xFFFFu) + (a3 << 16);
  const std::uint32_t frameBias = memory_.r16(at + 0x10);
  v1_ = memory_.r32(at + 0xC);
  depth -= frameBias >> 8;
  if (asSigned(depth) < 0) {
    return Outcome::Skipped;
  }
  const std::uint32_t frame = quad.frames + ((frameBias & 0xFFu) << 3);
  const std::uint32_t first = memory_.r32(frame + 4);
  const std::uint32_t second = memory_.r32(frame + 8);
  const std::uint32_t s0 = gte.readData(gte::kSxy0);
  gte.writeData(gte::kVxy0, v0_);
  gte.writeData(gte::kVz0, 0);
  v0_ = gte.readData(gte::kSxy1);
  const std::uint32_t s2 = gte.readData(gte::kSxy2);
  gte.execute(gte::kRtps);
  memory_.w32(cursor_ + 0x08, s0);
  memory_.w32(cursor_ + 0x10, v0_);
  memory_.w32(cursor_ + 0x18, s2);
  memory_.w32(cursor_ + 0x04, v1_);
  memory_.w32(cursor_, kQuadTag);
  writeUvs(first, second);
  memory_.w32(cursor_ + 0x20, gte.readData(gte::kSxy2));
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    gte.writeControl(gte::kRotation + i, rotation[i]);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    gte.writeControl(gte::kTranslationX + i, 0);
  }
  link(depth << 3, Packet::Quad);
  return Outcome::Drawn;
}

// 80057E7C: a line between two camera-relative points, tested at its first.
Outcome EmitList::line(std::uint32_t at) {
  auto &gte = core_.gte();
  std::uint32_t x = memory_.r32(at + 4);
  const std::uint32_t a0 = memory_.r32(at + 8);
  std::uint32_t v0 = camera_[1] - (x >> 16);
  x = (x & 0xFFFFu) - camera_[0];
  const bool poolFull = asSigned(limit_ - cursor_) <= 0;
  v1_ = a0 & 0xFFFFu;
  if (poolFull) {
    return Outcome::PoolFull;
  }
  v0 += (camera_[2] - v1_) << 16;
  gte.writeData(gte::kVxy0, v0);
  gte.writeData(gte::kVz0, x);
  x = (a0 >> 16) - camera_[0];
  gte.execute(gte::kRtps);
  const std::uint32_t second = memory_.r32(at + 0xC);
  v0 = camera_[1] - (second & 0xFFFFu);
  v1_ = (camera_[2] - (second >> 16)) << 16;
  memory_.w8(at + record::kSeen, 0);
  v0 += v1_;
  gte.writeData(gte::kVxy0, v0);
  gte.writeData(gte::kVz0, x);
  v0_ = gte.readData(gte::kSz3);
  const std::uint32_t sxy = gte.readData(gte::kSxy2);
  gte.execute(gte::kRtps);
  const std::uint32_t colour = memory_.r32(at + 0x10);
  const std::uint32_t depth = v0_;
  v1_ = depth - 0x2000;
  if (depth == 0) {
    return Outcome::Skipped;
  }
  if (asSigned(v1_) >= 0) {
    v1_ = sxy - kTop;
    return Outcome::Skipped;
  }
  v1_ = sxy - kTop;
  if (asSigned(v1_) <= 0) {
    v1_ = sxy - kBottom;
    return Outcome::Skipped;
  }
  v1_ = sxy - kBottom;
  if (asSigned(v1_) >= 0) {
    return Outcome::Skipped;
  }
  std::uint32_t colourBias = memory_.r32(at + 0x14);
  v1_ = (sxy << 16) - rightEdge_;
  if (leftOf(sxy)) {
    return Outcome::Skipped;
  }
  v0_ = depth >> 5;
  if (rightOf(sxy)) {
    return Outcome::Skipped;
  }
  v1_ = gte.readData(gte::kSxy2);
  memory_.w32(cursor_ + 0x08, sxy);
  memory_.w32(cursor_ + 0x10, v1_);
  memory_.w32(cursor_ + 0x04, colour);
  memory_.w32(cursor_ + 0x0C, colourBias);
  colourBias >>= 24;
  v0_ -= colourBias;
  if (asSigned(v0_ - 2) <= 0) {
    return Outcome::Skipped;
  }
  memory_.w32(cursor_, kLineTag);
  markSeen(at, retailDrops(sxy));
  v0_ <<= 3;
  v1_ = link(v0_, Packet::Line);
  return Outcome::Drawn;
}

// 80057F98: a one-pixel line at a camera-relative point.
Outcome EmitList::dot(std::uint32_t at) {
  auto &gte = core_.gte();
  std::uint32_t x = memory_.r32(at + 4);
  const std::uint32_t a0 = memory_.r32(at + 8);
  std::uint32_t v0 = camera_[1] - (x >> 16);
  x = (x & 0xFFFFu) - camera_[0];
  const bool poolFull = asSigned(limit_ - cursor_) <= 0;
  v1_ = a0 & 0xFFFFu;
  if (poolFull) {
    return Outcome::PoolFull;
  }
  v0 += (camera_[2] - v1_) << 16;
  gte.writeData(gte::kVxy0, v0);
  gte.writeData(gte::kVz0, x);
  const std::uint32_t bias = (a0 >> 16) & 0xFFu;
  gte.execute(gte::kRtps);
  memory_.w32(cursor_ + 4, memory_.r32(at + 0xC));
  memory_.w8(at + record::kSeen, 0);
  v1_ = gte.readData(gte::kSz3);
  v0_ = gte.readData(gte::kSxy2);
  const std::uint32_t sxy = v0_;
  if (v1_ == 0 || asSigned(v1_ - 0x2000) >= 0 || asSigned(sxy - kTop) <= 0 ||
      asSigned(sxy - kBottom) >= 0 || leftOf(sxy)) {
    return Outcome::Skipped;
  }
  v1_ >>= 5;
  if (rightOf(sxy)) {
    return Outcome::Skipped;
  }
  markSeen(at, retailDrops(sxy));
  memory_.w32(cursor_ + 0x08, sxy);
  v0_ = sxy + 1;
  memory_.w32(cursor_ + 0x0C, v0_);
  v0_ = kDotTag;
  memory_.w32(cursor_, v0_);
  v1_ -= bias;
  v0_ = v1_ - 2;
  const bool behind = asSigned(v0_) <= 0;
  v1_ <<= 3;
  if (behind) {
    return Outcome::Skipped;
  }
  v1_ = link(v1_, Packet::Dot);
  return Outcome::Drawn;
}

} // namespace spyro1::particles
