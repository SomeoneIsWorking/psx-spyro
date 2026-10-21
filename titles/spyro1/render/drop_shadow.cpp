#include "spyro1/render/drop_shadow.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_facts.h"

namespace spyro1::shadow {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::signedHalf;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

constexpr std::uint32_t kSpyro = actors::kSpyroRecord;
constexpr std::uint32_t kHeading = actors::spyro_record::kAngles + 2;
constexpr std::uint32_t kAnimation = actors::spyro_record::kAnimations;
constexpr std::uint32_t kFrame = actors::spyro_record::kFrames;
constexpr std::uint32_t kBlend = actors::spyro_record::kBlends;
// The model's animation headers and a header's frame words.
constexpr std::uint32_t kAnimationTable = 0x38u;
constexpr std::uint32_t kFrameTable = 0x24u;

constexpr std::uint32_t kRadii = kStaging + 0x80u;
constexpr std::uint32_t kHeights = kStaging + 0x90u;
constexpr std::uint32_t kPoints = 16;

// A segment: a draw-mode packet (subtractive, dithered), then a POLY_G3 from the centre.
constexpr std::uint32_t kModeTag = 0x82000000u;
constexpr std::uint32_t kDrawMode = 0xE1000640u;
constexpr std::uint32_t kTriangleCentre = 0x32608080u;
constexpr std::uint32_t kSegmentBytes = 0x28u;

// A frame word's data address (80059C1C).
std::uint32_t frameData(std::uint32_t word) {
  return (word << 11) >> 10;
}

} // namespace

DropShadow::DropShadow(Core &core, const psx::present::EmitMemory &memory)
    : core_(core), memory_(memory) {}

bool DropShadow::draw() {
  if (asSigned(memory_.mem_r32(shadow_block::kSuppress)) > 0) {
    v0_ = memory_.mem_r32(shadow_block::kAnchor + 4);
    v1_ = memory_.mem_r32(shadow_block::kAnchor + 8);
    return false;
  }
  placeCentre();
  stageRadii();
  stageRing();
  projectRing();
  drawSegments();
  return true;
}

void DropShadow::end() {
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

void DropShadow::placeCentre() {
  auto &gte = core_.gte();
  const std::uint32_t position = kRenderGlobals.cameraPosition;
  const std::uint32_t x = memory_.mem_r32(shadow_block::kAnchor) - memory_.mem_r32(position);
  const std::uint32_t y = memory_.mem_r32(position + 4) - memory_.mem_r32(shadow_block::kAnchor + 4);
  const std::uint32_t z = memory_.mem_r32(position + 8) - memory_.mem_r32(shadow_block::kAnchor + 8);
  const psx::gte::RotationWords camera = spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation);
  gte.loadRotation(camera);
  gte.loadTranslation({});
  gte.writeData(gte::kVz0, x);
  gte.writeData(gte::kVxy0, (y & 0xFFFFu) + (z << 16));
  gte.execute(gte::kRtps);
  const std::uint32_t heading = static_cast<std::uint32_t>(memory_.mem_r8(kSpyro + kHeading)) << 1;
  centreScreen_ = gte.readData(gte::kSxy2);
  centreDepth_ = gte.readData(gte::kSz3);
  gte.writeControl(gte::kTranslationZ, gte.readData(gte::kMac3));
  gte.writeControl(gte::kTranslationY, gte.readData(gte::kMac2));
  gte.writeControl(gte::kTranslationX, gte.readData(gte::kMac1));
  const std::uint32_t cosine = memory_.mem_r16(kRenderGlobals.cameraSineTable + spyro::guest_render_globals::kCosineOffset + heading);
  const std::uint32_t sine = memory_.mem_r16(kRenderGlobals.cameraSineTable + heading);
  gte.writeData(gte::kVxy0, cosine);
  gte.writeData(gte::kVz0, sine);
  gte.execute(gte::kMvmvaRtV0);
  const std::uint32_t across[3] = {
      gte.readData(gte::kIr1), gte.readData(gte::kIr2), gte.readData(gte::kIr3)};
  gte.writeData(gte::kVxy0, (0u - sine) & 0xFFFFu);
  gte.writeData(gte::kVz0, cosine);
  gte.execute(gte::kMvmvaRtV0);
  const std::uint32_t along[3] = {
      gte.readData(gte::kIr1), gte.readData(gte::kIr2), gte.readData(gte::kIr3)};
  gte.writeControl(gte::kRotation + 0, (camera[0] & 0xFFFF0000u) + (across[0] & 0xFFFFu));
  gte.writeControl(gte::kRotation + 1, (along[0] & 0xFFFFu) + (across[1] << 16));
  gte.writeControl(gte::kRotation + 2, (camera[2] & 0xFFFFu) + (along[1] << 16));
  gte.writeControl(gte::kRotation + 3, (camera[3] & 0xFFFF0000u) + (across[2] & 0xFFFFu));
  gte.writeControl(gte::kRotation + 4, along[2] & 0xFFFFu);
}

void DropShadow::stageRadii() {
  auto &gte = core_.gte();
  const std::uint32_t model = memory_.mem_r32(actors::kSpyroModel);
  const std::uint32_t blend = memory_.mem_r8(kSpyro + kBlend);
  std::uint32_t frame = 0;
  if (blend == 0) {
    const std::uint32_t animation = memory_.mem_r8(kSpyro + kAnimation);
    const std::uint32_t header = memory_.mem_r32(model + (animation << 2) + kAnimationTable);
    frame = frameData(memory_.mem_r32(header + (memory_.mem_r8(kSpyro + kFrame) << 2) + kFrameTable));
  } else {
    const std::uint32_t animations = memory_.mem_r16(kSpyro + kAnimation);
    const std::uint32_t frames = memory_.mem_r16(kSpyro + kFrame);
    const std::uint32_t header = memory_.mem_r32(model + ((animations & 0xFFu) << 2) + kAnimationTable);
    const std::uint32_t partnerHeader =
        memory_.mem_r32(model + ((animations >> 8) << 2) + kAnimationTable);
    const std::uint32_t first =
        frameData(memory_.mem_r32(header + ((frames & 0xFFu) << 2) + kFrameTable));
    const std::uint32_t partner =
        frameData(memory_.mem_r32(partnerHeader + ((frames >> 8) << 2) + kFrameTable));
    memory_.mem_w32(kStaging + 0x0, memory_.mem_r32(first));
    memory_.mem_w32(kStaging + 0x4, memory_.mem_r32(first + 4));
    memory_.mem_w32(kStaging + 0x8, memory_.mem_r32(partner));
    memory_.mem_w32(kStaging + 0xC, memory_.mem_r32(partner + 4));
    // 80059C5C: three passes of three bytes, each IR0 = 16 - blend toward the partner's.
    for (std::uint32_t at = kStaging; at != kStaging + 9; at += 3) {
      gte.writeData(gte::kIr0, 0x10u - blend);
      for (std::uint32_t i = 0; i < 3; ++i) {
        gte.writeData(gte::kIr1 + i, memory_.mem_r8(at + i));
      }
      gte.execute(gte::kGpf);
      gte.writeData(gte::kIr0, blend);
      for (std::uint32_t i = 0; i < 3; ++i) {
        gte.writeData(gte::kIr1 + i, memory_.mem_r8(at + 8 + i));
      }
      gte.execute(gte::kGpl);
      for (std::uint32_t i = 0; i < 3; ++i) {
        memory_.mem_w8(at + i, static_cast<std::uint8_t>(gte.readData(gte::kMac1 + i) >> 4));
      }
    }
    frame = kStaging;
  }
  const std::uint32_t second = memory_.mem_r32(frame + 4);
  const std::uint32_t firstWord = memory_.mem_r32(frame);
  memory_.mem_w32(kStaging + 4, second);
  memory_.mem_w32(kStaging + 0, firstWord);
  memory_.mem_w32(kStaging + 8, firstWord);
}

void DropShadow::stageRing() {
  const std::uint32_t heights = memory_.mem_r32(shadow_block::kHeights);
  const bool airborne =
      (memory_.mem_r32(shadow_block::kSlotFlags) | memory_.mem_r32(shadow_block::kSlotFlags + 4)) != 0;
  memory_.mem_w32(kStaging + 0x10, memory_.mem_r32(shadow_block::kHeights + 4));
  memory_.mem_w32(kStaging + 0x0C, heights);
  memory_.mem_w32(kStaging + 0x14, heights);
  const auto shrink = [airborne](std::int32_t value) {
    return airborne ? (value + (value << 1)) >> 2 : value;
  };
  for (std::uint32_t i = 0; i < 8; ++i) {
    const std::int32_t radius = shrink(memory_.mem_r8(kStaging + i));
    const std::int32_t next = shrink(memory_.mem_r8(kStaging + i + 1));
    const std::int32_t height = shrink(static_cast<std::int8_t>(memory_.mem_r8(kStaging + 0xC + i)));
    const std::int32_t nextHeight =
        shrink(static_cast<std::int8_t>(memory_.mem_r8(kStaging + 0xD + i)));
    memory_.mem_w8(kRadii + 2 * i, static_cast<std::uint8_t>(radius));
    memory_.mem_w8(kRadii + 2 * i + 1,
               static_cast<std::uint8_t>(static_cast<std::uint32_t>(radius + next) >> 1));
    memory_.mem_w8(kHeights + 2 * i, static_cast<std::uint8_t>(height));
    memory_.mem_w8(kHeights + 2 * i + 1, static_cast<std::uint8_t>((height + nextHeight) >> 1));
  }
}

void DropShadow::projectRing() {
  auto &gte = core_.gte();
  for (std::uint32_t point = 0; point < kPoints; ++point) {
    const std::uint32_t pair = memory_.mem_r32(kRing + point * 4);
    const std::uint32_t radius = memory_.mem_r8(kRadii + point);
    const std::uint32_t height = memory_.mem_r8(kHeights + point);
    const std::uint32_t z = sra(radius * signedHalf(pair), 10);
    const std::uint32_t x = sra(radius * sra(pair, 16), 10);
    gte.writeData(gte::kVz0, z);
    gte.writeData(gte::kVxy0, (x & 0xFFFFu) + (sra(height << 24, 24) << 18));
    gte.execute(gte::kRtps);
    memory_.mem_w32(kStaging + point * 8, gte.readData(gte::kSxy2));
    memory_.mem_w32(kStaging + point * 8 + 4, gte.readData(gte::kSz3));
  }
}

void DropShadow::drawSegments() {
  const std::uint32_t table = memory_.mem_r32(kRenderGlobals.orderingTable);
  std::uint32_t cursor = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t bias = memory_.mem_r32(shadow_block::kBias);
  const std::uint32_t closing = kStaging + kPoints * 8;
  memory_.mem_w32(closing, memory_.mem_r32(kStaging));
  memory_.mem_w32(closing + 4, memory_.mem_r32(kStaging + 4));
  for (std::uint32_t at = kStaging; at != closing; at += 8) {
    const std::uint32_t screen = memory_.mem_r32(at);
    const std::uint32_t nextScreen = memory_.mem_r32(at + 8);
    const std::uint32_t rimDepth = memory_.mem_r32(at + 4) + memory_.mem_r32(at + 0xC);
    memory_.mem_w32(cursor + 0x10, kTriangleCentre);
    memory_.mem_w32(cursor + 0x14, centreScreen_);
    memory_.mem_w32(cursor + 0x18, 0);
    memory_.mem_w32(cursor + 0x1C, screen);
    memory_.mem_w32(cursor + 0x20, 0);
    memory_.mem_w32(cursor + 0x24, nextScreen);
    memory_.mem_w32(cursor + 0x08, 0);
    memory_.mem_w32(cursor + 0x04, kDrawMode);
    memory_.mem_w32(cursor, (kModeTag ^ cursor) + 0xC);
    memory_.mem_w32(cursor + 0x0C, spyro::guest_packet::kG3Tag);
    const std::uint32_t bin = ((rimDepth + (rimDepth >> 1) + centreDepth_) >> 9) - bias;
    if (asSigned(bin) < 0) {
      continue;
    }
    const std::uint32_t slot = (bin << 3) + table;
    spyro::depth_bins::linkIntoBin(memory_, slot, cursor, cursor + 0xC);
    cursor += kSegmentBytes;
  }
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor);
  v0_ = kRadii;
  v1_ = centreScreen_;
}

} // namespace spyro1::shadow
