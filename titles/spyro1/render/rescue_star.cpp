#include "spyro1/render/rescue_star.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "spyro1/render/terrain_facts.h"
#include "state_producer.h"

namespace spyro1::rescue_star {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

constexpr std::uint32_t kSpikes = kStaging + 0x24u;
// The draw mode: additive, no dither; then POLY_F3s, semi-transparent.
constexpr std::uint32_t kModeTag = 0x02000000u;
constexpr std::uint32_t kDrawMode = 0xE1000620u;
constexpr std::uint32_t kModeBytes = 0xCu;
constexpr std::uint32_t kTriangleTag = 0x04000000u;
constexpr std::uint32_t kTriangleCode = 0x22u;
constexpr std::uint32_t kTriangleBytes = 0x14u;
// Angle steps between points in the sine table, and the spikes' half-step offset.
constexpr std::uint32_t kStep = 0x40u;
constexpr std::uint32_t kLastInner = 0x1C0u;
constexpr std::uint32_t kLastSpike = 0x1E0u;

} // namespace

RescueStar::RescueStar(Core &core,
                       const psx::present::EmitMemory &memory,
                       psx::present::PrimitiveSink *places)
    : core_(core), memory_(memory), places_(places) {}

std::uint32_t RescueStar::draw() {
  placeCentre();
  projectPoints();
  return drawTriangles();
}

void RescueStar::end() {
  core_.r[2] = kStaging + kStagingBytes;
  core_.r[3] = kStaging + kStagingBytes;
}

void RescueStar::placeCentre() {
  auto &gte = core_.gte();
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation));
  const std::uint32_t camera = kRenderGlobals.cameraPosition;
  gte.writeData(gte::kIr3, memory_.mem_r32(kPosition) - memory_.mem_r32(camera));
  gte.writeData(gte::kIr1, memory_.mem_r32(camera + 4) - memory_.mem_r32(kPosition + 4));
  gte.writeData(gte::kIr2, memory_.mem_r32(camera + 8) - memory_.mem_r32(kPosition + 8));
  gte.execute(gte::kMvmvaRtIr);
  gte.writeControl(gte::kTranslationX, gte.readData(gte::kMac1));
  gte.writeControl(gte::kTranslationY, gte.readData(gte::kMac2));
  gte.writeControl(gte::kTranslationZ, gte.readData(gte::kMac3));
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kRotation));
  gte.writeData(gte::kVxy0, 0);
  gte.writeData(gte::kVz0, 0);
  gte.execute(gte::kRtps);
  centreScreen_ = gte.readData(gte::kSxy2);
  centrePlace_ = gte.projectedPlace(2);
}

void RescueStar::projectPoints() {
  auto &gte = core_.gte();
  const std::uint32_t scale = memory_.mem_r32(kScale);
  const std::uint32_t sine = kRenderGlobals.cameraSineTable;
  const std::uint32_t cosine = sine + spyro::guest_render_globals::kCosineOffset;
  const auto project = [&](std::uint32_t angle, std::uint32_t shift) {
    gte.writeData(
        gte::kVz0,
        sra(spyro::guest_word::signedHalf(memory_.mem_r16(cosine + angle)) * scale, shift));
    gte.writeData(gte::kVxy0,
                  sra(spyro::guest_word::signedHalf(memory_.mem_r16(sine + angle)) * scale, shift));
    gte.execute(gte::kRtps);
    return gte.readData(gte::kSxy2);
  };
  // The inner ring runs from 0x1C0 down to 0 into slots 1..8; slot 0 repeats the last.
  for (std::uint32_t slot = 1; slot <= kPoints; ++slot) {
    memory_.mem_w32(kStaging + 4 * slot, project(kLastInner - (slot - 1) * kStep, 12));
    innerPlaces_[slot] = gte.projectedPlace(2);
  }
  memory_.mem_w32(kStaging, memory_.mem_r32(kStaging + 4 * kPoints));
  innerPlaces_[0] = innerPlaces_[kPoints];
  for (std::uint32_t slot = 0; slot < kPoints; ++slot) {
    memory_.mem_w32(kSpikes + 4 * slot, project(kLastSpike - slot * kStep, 10));
    spikePlaces_[slot] = gte.projectedPlace(2);
  }
}

std::uint32_t RescueStar::drawTriangles() {
  const std::uint32_t pair = memory_.mem_r32(kRenderGlobals.overlayPair);
  std::uint32_t cursor = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t first = cursor;
  // The pair is {tail, head}: a packet is linked after the tail, or becomes the head.
  const auto append = [&](std::uint32_t packet) {
    const std::uint32_t tail = memory_.mem_r32(pair);
    memory_.mem_w32(pair, packet);
    if (tail == 0) {
      memory_.mem_w32(pair + 4, packet);
      return;
    }
    memory_.mem_w16(tail, static_cast<std::uint16_t>(packet));
    memory_.mem_w8(tail + 2, static_cast<std::uint8_t>(packet >> 16));
  };
  memory_.mem_w32(cursor + 8, 0);
  memory_.mem_w32(cursor + 4, kDrawMode);
  memory_.mem_w32(cursor, kModeTag);
  append(cursor);
  cursor += kModeBytes;
  const std::uint32_t grey = memory_.mem_r8(kColour);
  const std::uint32_t colour = (kTriangleCode << 24) | (grey << 16) | (grey << 8) | grey;
  for (std::uint32_t point = 0; point < kPoints; ++point) {
    const std::uint32_t inner = memory_.mem_r32(kStaging + 4 * point);
    const std::uint32_t next = memory_.mem_r32(kStaging + 4 * point + 4);
    const std::uint32_t spike = cursor;
    const std::uint32_t fan = cursor + kTriangleBytes;
    memory_.mem_w32(spike + 4, colour);
    memory_.mem_w32(spike + 8, inner);
    memory_.mem_w32(spike + 0xC, next);
    memory_.mem_w32(spike + 0x10, memory_.mem_r32(kSpikes + 4 * point));
    memory_.mem_w32(fan + 4, colour);
    memory_.mem_w32(fan + 8, inner);
    memory_.mem_w32(fan + 0xC, next);
    memory_.mem_w32(fan + 0x10, centreScreen_);
    memory_.mem_w32(spike, kTriangleTag);
    memory_.mem_w32(fan, kTriangleTag);
    if (places_ != nullptr) {
      places_->placeCorners(spike,
                            {{innerPlaces_[point], innerPlaces_[point + 1], spikePlaces_[point]}});
      places_->placeCorners(fan, {{innerPlaces_[point], innerPlaces_[point + 1], centrePlace_}});
    }
    append(spike);
    append(fan);
    cursor += 2 * kTriangleBytes;
  }
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor);
  return first;
}

} // namespace spyro1::rescue_star
