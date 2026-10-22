#include "spyro1/render/moby_shadows.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_word.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/terrain_facts.h"
#include "state_producer.h"

#include <array>

namespace spyro1::shadows {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::signedHalf;
namespace moby_record = spyro::moby_record;
using terrain::kRenderGlobals;

constexpr std::uint32_t kCameraQuarters = kStaging;
constexpr std::uint32_t kRotation = kStaging + 0x0Cu;
constexpr std::uint32_t kTextureWords = kStaging + 0x20u;
constexpr std::uint32_t kCursor = kStaging + 0x28u;
constexpr std::uint32_t kTable = kStaging + 0x2Cu;
// Four rim points and the first again, then their depths.
constexpr std::uint32_t kPoints = kStaging + 0x30u;
constexpr std::uint32_t kClosingPoint = kPoints + 0x10u;
constexpr std::uint32_t kDepths = kPoints + 0x14u;

// A disc further than this is not drawn; past kFadeDepth it fades toward kFadeEnd.
constexpr std::int32_t kFarDepth = 0x1000;
constexpr std::int32_t kFadeDepth = 0xC00;
constexpr std::uint32_t kFadeEnd = 0x400u;
constexpr std::int32_t kNudgeDepth = 0x400;
constexpr std::uint32_t kGrey = 0x808080u;
// The third vertex's v: 0x1F rows below the first's.
constexpr std::uint32_t kThirdTexel = 0x1F00u;
constexpr std::uint32_t kOneRow = 0x10000u;
// 8005A330..8005A354: the centre must lie above row 240 and within 8 columns of the presented
// window.
constexpr std::uint32_t kBottomRow = 0xF00000u;
constexpr std::int32_t kEdgeSlack = 8;

} // namespace

MobyShadows::MobyShadows(Core &core,
                         const psx::present::EmitMemory &memory,
                         const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds) {}

void MobyShadows::begin() {
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    memory_.mem_w32(kCameraQuarters + axis * 4,
                    memory_.mem_r32(kRenderGlobals.cameraPosition + axis * 4) >> 2);
  }
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    memory_.mem_w32(kRotation + i * 4, memory_.mem_r32(kRenderGlobals.cameraRotation + i * 4));
  }
  memory_.mem_w32(kTextureWords, memory_.mem_r32(actors::kShadowListHolder));
  memory_.mem_w32(kTextureWords + 4, memory_.mem_r32(actors::kShadowListHolder + 4));
  memory_.mem_w32(kCursor, memory_.mem_r32(kRenderGlobals.primitiveCursor));
  memory_.mem_w32(kTable, memory_.mem_r32(kRenderGlobals.orderingTable));
}

void MobyShadows::end() {
  memory_.mem_w32(kRenderGlobals.primitiveCursor, memory_.mem_r32(kCursor));
}

void MobyShadows::draw(std::uint32_t record, std::uint32_t radius) {
  auto &gte = core_.gte();
  psx::gte::RotationWords r = spyro::guest_memory::rotationAt(memory_, kRotation);
  gte.loadRotation(r);
  gte.loadTranslation({});
  const std::uint32_t ground = memory_.mem_r32(record + kGround);
  if ((ground & 0xFFFFu) == 0) {
    return;
  }
  const std::uint32_t height = (memory_.mem_r32(kCameraQuarters + 8) - (ground & 0xFFFFu)) << 16;
  const std::uint32_t across =
      memory_.mem_r32(kCameraQuarters + 4) - (memory_.mem_r32(record + moby_record::kY) >> 2);
  gte.writeData(gte::kVxy0, height + across);
  gte.writeData(gte::kVz0,
                (memory_.mem_r32(record + moby_record::kX) >> 2) -
                    memory_.mem_r32(kCameraQuarters));
  gte.execute(gte::kRtps);
  const std::uint32_t slopeX = ((ground >> 16) & 0x3Fu) << 2;
  const std::uint32_t slopeZ = ((ground >> 22) & 0x3Fu) << 2;
  const std::uint32_t depth = gte.readData(gte::kMac3);
  gte.loadTranslation({gte.readData(gte::kMac1), gte.readData(gte::kMac2), depth});
  if (asSigned(depth) - kFarDepth >= 0) {
    return;
  }
  // 8005A108..8005A238: tilted by the two slopes, the byte-1 and byte-0 axes of a moby's angles.
  (void)spyro::guest_moby_rotation::composeTabled(
      core_,
      guest::kSinTable,
      {.angles = (slopeX << 8) | slopeZ},
      r,
      spyro::guest_moby_rotation::Byte0Result::Stored,
      spyro::guest_moby_rotation::Byte1Rt12::Byte0Offset);
  gte.loadRotation(r);
  const psx::gte::ProjectedVertex centreVertex = gte.projected(2);
  const std::uint32_t centre = centreVertex.word();
  const std::uint32_t centreDepth = gte.readData(gte::kSz3);
  gte.writeData(gte::kVxy0, 0);
  gte.writeData(gte::kVz0, radius);
  gte.writeData(gte::kVxy1, radius);
  gte.writeData(gte::kVz1, 0);
  gte.writeData(gte::kVxy2, 0);
  gte.writeData(gte::kVz2, 0u - radius);
  const std::int32_t fade = asSigned(gte.readControl(gte::kTranslationZ)) - kFadeDepth;
  gte.execute(gte::kRtpt);
  std::uint32_t command = spyro::guest_packet::kSemiFt3Code | kGrey;
  if (fade >= 0) {
    const std::uint32_t level = (kFadeEnd - static_cast<std::uint32_t>(fade)) >> 3;
    command = spyro::guest_packet::kSemiFt3Code | level | (level << 8) | (level << 16);
  }
  const psx::gte::ProjectedVertex first = gte.projected(0);
  memory_.storeVertex(kPoints, first);
  memory_.storeVertex(kPoints + 4, gte.projected(1));
  memory_.storeVertex(kPoints + 8, gte.projected(2));
  memory_.storeVertex(kClosingPoint, first);
  gte.execute(gte::kNclip);
  const std::uint32_t firstDepth = gte.readData(gte::kSz1);
  memory_.mem_w32(kDepths, firstDepth);
  memory_.mem_w32(kDepths + 4, gte.readData(gte::kSz2));
  if (asSigned(gte.readData(gte::kMac0)) < 0) {
    return;
  }
  memory_.mem_w32(kDepths + 8, gte.readData(gte::kSz3));
  memory_.mem_w32(kDepths + 0x10, firstDepth);
  gte.writeData(gte::kVxy0, (0u - radius) & 0xFFFFu);
  gte.writeData(gte::kVz0, 0);
  const std::uint32_t step = static_cast<std::uint32_t>(memory_.mem_r8s(record + kDepthStep) - 1);
  gte.execute(gte::kRtps);
  if (asSigned(centre) <= 0 || asSigned(centre - kBottomRow) >= 0) {
    return;
  }
  const std::int32_t x = asSigned(signedHalf(centre));
  if (x + kEdgeSlack <= bounds_.left() || x - kEdgeSlack - bounds_.right() >= 0) {
    return;
  }
  memory_.storeVertex(kPoints + 0xC, gte.projected(2));
  memory_.mem_w32(kDepths + 0xC, gte.readData(gte::kSz3));
  // 8005A370..8005A3B8: a near disc's rim rows step one toward the centre's depth.
  if (asSigned(gte.readControl(gte::kTranslationZ)) - kNudgeDepth > 0) {
    for (std::uint32_t at = kPoints; at != kDepths; at += 4) {
      const std::int32_t behind = asSigned(memory_.mem_r32(at + 0x14) - centreDepth);
      const psx::gte::ProjectedVertex point = memory_.loadVertex(at);
      if (behind < 0) {
        memory_.storeVertex(at, point.steppedTo(point.word() + kOneRow));
      } else if (behind != 0) {
        memory_.storeVertex(at, point.steppedTo(point.word() - kOneRow));
      }
    }
  }
  const std::uint32_t uv0 = memory_.mem_r32(kTextureWords);
  const std::uint32_t uv1 = memory_.mem_r32(kTextureWords + 4);
  const std::uint32_t table = memory_.mem_r32(kTable);
  std::uint32_t cursor = memory_.mem_r32(kCursor);
  for (std::uint32_t at = kPoints; at != kClosingPoint; at += 4) {
    memory_.mem_w32(cursor + 0x04, command);
    memory_.storeVertex(cursor + 0x08, centreVertex);
    memory_.mem_w32(cursor + 0x0C, uv0);
    memory_.mem_w32(cursor + 0x14, uv1);
    memory_.mem_w32(cursor + 0x1C, uv0 + kThirdTexel);
    memory_.storeVertex(cursor + 0x10, memory_.loadVertex(at));
    memory_.storeVertex(cursor + 0x18, memory_.loadVertex(at + 4));
    memory_.mem_w32(cursor, spyro::guest_packet::kFt3Tag);
    std::uint32_t rim = memory_.mem_r32(at + 0x14) + memory_.mem_r32(at + 0x18);
    rim += rim >> 1;
    const std::uint32_t bin = ((rim + centreDepth) >> 7) - step;
    if (asSigned(bin) < 0) {
      continue;
    }
    spyro::depth_bins::linkIntoBin(memory_, (bin << 3) + table, cursor, cursor);
    cursor += spyro::guest_packet::kFt3Bytes;
  }
  memory_.mem_w32(kCursor, cursor);
}

} // namespace spyro1::shadows
