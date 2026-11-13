#include "spyro1/core/debug/teleport.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "core/debug/moby_teleport.h"
#include "render/field/moby_record.h"
#include <cmath>
#include <cstdint>
#include <lucent/log.h>
#include <numbers>
#include <optional>
#include <vector>

namespace spyro1::teleport {
namespace {

using spyro1::guest::kGamestate;
using spyro1::guest::kLevelId;
using spyro1::guest::kLevelMobys;
using spyro1::guest::kLoadStage;
using spyro1::guest::kMobyProps;
using spyro1::guest::kNextLevelId;
using spyro1::guest::kPortalCount;
using spyro1::guest::kPortals;
using spyro1::guest::kSpyro;

constexpr uint32_t kPortalPathMoby = 0x18u;
constexpr uint32_t kPortalLevelId = 0x1cu;
constexpr uint32_t kPortalFirstPoint = 0x20u;
constexpr uint32_t kPathData = 0x00u;
constexpr uint32_t kPathNodeCount = 0x00u;
constexpr uint32_t kPathNodes = 0x08u;
constexpr uint32_t kPathNodeStride = 0x10u;
constexpr uint32_t kSpyroPosition = 0x00u;
constexpr uint32_t kSpyroPreviousPosition = 0x8cu;
// m_Physics.m_TargetSpeedAngle.m_RotZ and m_SpeedAngle.m_RotZ: the heading the body and camera
// follow.
constexpr uint32_t kSpyroTargetHeading = 0xccu;
constexpr uint32_t kSpyroHeading = 0x11cu;
constexpr uint32_t kHeadingMask = kHeadingTurn - 1u;
// SPY.bodyRotZ, the 8-bit facing the talker's check reads.
constexpr uint32_t kSpyroBodyYaw = 0x0eu;
constexpr uint32_t kMobyYaw = 0x46u;            // actor rotZ, 8 bits a turn
constexpr uint32_t kMobyMode = 0x48u;           // actor mode
constexpr uint8_t kTalkerReady = 1u;            // TALKER_READY
constexpr uint32_t kMobyUpdateDistance = 0x52u; // activation radius >> 10
constexpr double kTalkerRearmRange = 0x1000;    // TALKER_REARM_RANGE
constexpr uint32_t kYawTurn = 0x100u;
constexpr uint32_t kHeadingPerYaw = kHeadingTurn / kYawTurn;
// Inside TALKER_TALK_RANGE (0x780) on the talker's facing (within TALKER_FACING_ANGLE).
constexpr double kTalkerDistance = 0x400;
constexpr uint32_t kMaxPortals = 16u;
constexpr uint32_t kMaxPathNodes = 16u;

using Position = spyro::MobyTeleport::Position;

struct GateInfo {
  int32_t pathMoby = -1;
  int32_t levelId = -1;
  Position firstPoint;
  uint32_t nodeCount = 0;
  Position nodes[kMaxPathNodes]{};
};

bool validPointer(Core &core, uint32_t address, uint32_t bytes = 4u) {
  return address != 0u && core.isGuestStorage(address, bytes);
}

bool readPointer(Core &core, uint32_t address, uint32_t &out) {
  if (!core.isGuestStorage(address, 4u)) {
    return false;
  }
  out = core.mem_r32(address);
  return validPointer(core, out);
}

Position readPosition(Core &core, uint32_t address) {
  return {static_cast<int32_t>(core.mem_r32(address)),
          static_cast<int32_t>(core.mem_r32(address + 4u)),
          static_cast<int32_t>(core.mem_r32(address + 8u))};
}

std::optional<GateInfo> readGatePath(Core &core, uint32_t portal) {
  if (!validPointer(core, portal) || !core.isGuestStorage(portal + kPortalFirstPoint, 12u)) {
    return std::nullopt;
  }
  const int32_t pathMoby = static_cast<int32_t>(core.mem_r32(portal + kPortalPathMoby));
  const uint32_t levelMobys = core.mem_r32(kLevelMobys);
  if (pathMoby < 0 || !validPointer(core, levelMobys)) {
    return std::nullopt;
  }
  const uint64_t mobyOffset =
      static_cast<uint64_t>(static_cast<uint32_t>(pathMoby)) * spyro::moby_record::kBytes;
  if (mobyOffset > 0xffffffffu) {
    return std::nullopt;
  }
  const uint32_t moby = levelMobys + static_cast<uint32_t>(mobyOffset);
  if (!core.isGuestStorage(moby, 4u)) {
    return std::nullopt;
  }
  const uint32_t props = core.mem_r32(moby + kMobyProps);
  uint32_t path = 0;
  if (!validPointer(core, props) || !readPointer(core, props + kPathData, path)) {
    return std::nullopt;
  }
  const uint32_t nodeCount = core.mem_r8(path + kPathNodeCount);
  if (nodeCount == 0u || nodeCount > kMaxPathNodes ||
      !core.isGuestStorage(path + kPathNodes, nodeCount * kPathNodeStride)) {
    return std::nullopt;
  }

  GateInfo out;
  out.pathMoby = pathMoby;
  out.levelId = static_cast<int32_t>(core.mem_r32(portal + kPortalLevelId));
  out.firstPoint = readPosition(core, portal + kPortalFirstPoint);
  out.nodeCount = nodeCount;
  for (uint32_t i = 0; i < nodeCount; ++i) {
    out.nodes[i] = readPosition(core, path + kPathNodes + i * kPathNodeStride);
  }
  return out;
}

std::optional<GateInfo> inspectGate(Core &core, uint32_t index) {
  const uint32_t count = core.mem_r32(kPortalCount);
  if (index >= count || index >= kMaxPortals) {
    return std::nullopt;
  }
  uint32_t portal = 0;
  if (!readPointer(core, kPortals + index * 4u, portal)) {
    return std::nullopt;
  }
  return readGatePath(core, portal);
}

void placeSpyro(Core &core, const Position &destination) {
  for (const uint32_t field : {kSpyroPreviousPosition, kSpyroPosition}) {
    core.mem_w32(kSpyro + field, static_cast<uint32_t>(destination.x));
    core.mem_w32(kSpyro + field + 4u, static_cast<uint32_t>(destination.y));
    core.mem_w32(kSpyro + field + 8u, static_cast<uint32_t>(destination.z));
  }
}

} // namespace

bool inActiveField(Core &core) {
  return core.mem_r32(kGamestate) == 0u && core.mem_r32(kLoadStage) == 0xffffffffu;
}

std::uint32_t LevelMobys::levelMobysWord() const {
  return kLevelMobys;
}

void LevelMobys::placeSpyro(Core &core, const Position &destination) const {
  teleport::placeSpyro(core, destination);
}

std::string toPortal(Core &core, uint32_t level) {
  const uint32_t count = core.mem_r32(kPortalCount);
  for (uint32_t i = 0; i < count && i < kMaxPortals; ++i) {
    const std::optional<GateInfo> gate = inspectGate(core, i);
    if (!gate || gate->levelId != static_cast<int32_t>(level)) {
      continue;
    }
    const Position destination = gate->nodes[0];
    placeSpyro(core, destination);
    lucent::info("spyro-teleport",
                 "portal gate={} targetLevel={} position=({}, {}, {})",
                 i,
                 level,
                 destination.x,
                 destination.y,
                 destination.z);
    return lucent::format("ok: Spyro in front of the portal into level {}", level);
  }
  return lucent::format("refused: no loaded portal leads to level {}", level);
}

std::string where(Core &core) {
  const Position at = readPosition(core, kSpyro + kSpyroPosition);
  return lucent::format("spyro position=({}, {}, {}) heading={}",
                        at.x,
                        at.y,
                        at.z,
                        core.mem_r32(kSpyro + kSpyroHeading) & kHeadingMask);
}

std::string toPosition(Core &core, const Position &destination, std::uint32_t heading) {
  placeSpyro(core, destination);
  core.mem_w32(kSpyro + kSpyroTargetHeading, heading & kHeadingMask);
  core.mem_w32(kSpyro + kSpyroHeading, heading & kHeadingMask);
  return lucent::format("ok: Spyro at ({}, {}, {}) heading {}",
                        destination.x,
                        destination.y,
                        destination.z,
                        heading & kHeadingMask);
}

namespace {

// Spyro `distance` out along the talker's facing, turned to face it.
std::string facingTalker(Core &core,
                         const LevelMobys &mobys,
                         std::uint32_t mobyClass,
                         std::optional<double> distance) {
  const std::vector<spyro::MobyTeleport::LevelMoby> talkers = mobys.ofClass(core, mobyClass);
  if (talkers.empty()) {
    return lucent::format("refused: the level has no moby of class {}", mobyClass);
  }
  const spyro::MobyTeleport::LevelMoby &talker = talkers.front();
  const uint32_t yaw = core.mem_r8(talker.address + kMobyYaw);
  // Halfway between the rearm range and the activation radius.
  const double out = distance.value_or(
      (kTalkerRearmRange + (core.mem_r8(talker.address + kMobyUpdateDistance) << 10u)) / 2.0);
  // Angle 0 is +x, a quarter turn +y, as the overlays' LookupCosine/LookupSine moves take it.
  const double radians = 2.0 * std::numbers::pi * yaw / kYawTurn;
  const Position at{
      .x = talker.position.x + static_cast<int32_t>(std::lround(out * std::cos(radians))),
      .y = talker.position.y + static_cast<int32_t>(std::lround(out * std::sin(radians))),
      .z = talker.position.z};
  const uint32_t facing = (yaw + kYawTurn / 2u) % kYawTurn;
  const std::string reply = toPosition(core, at, facing * kHeadingPerYaw);
  core.mem_w8(kSpyro + kSpyroBodyYaw, static_cast<uint8_t>(facing));
  lucent::info("spyro-teleport",
               "talker class={} moby={:#010x} yaw={} distance={}",
               mobyClass,
               talker.address,
               yaw,
               out);
  return reply;
}

} // namespace

std::string toTalker(Core &core, const LevelMobys &mobys, std::uint32_t mobyClass) {
  return facingTalker(core, mobys, mobyClass, kTalkerDistance);
}

std::string toTalkerApproach(Core &core, const LevelMobys &mobys, std::uint32_t mobyClass) {
  return facingTalker(core, mobys, mobyClass, std::nullopt);
}

bool talkerReady(Core &core, const LevelMobys &mobys, std::uint32_t mobyClass) {
  const std::vector<spyro::MobyTeleport::LevelMoby> talkers = mobys.ofClass(core, mobyClass);
  return !talkers.empty() && core.mem_r8(talkers.front().address + kMobyMode) == kTalkerReady;
}

std::string listGates(Core &core) {
  const uint32_t count = core.mem_r32(kPortalCount);
  std::string reply = lucent::format("level={} nextLevel={} gamestate={} loadStage={} portals={}",
                                     static_cast<int32_t>(core.mem_r32(kLevelId)),
                                     static_cast<int32_t>(core.mem_r32(kNextLevelId)),
                                     core.mem_r32(kGamestate),
                                     core.mem_r32(kLoadStage),
                                     count);
  for (uint32_t i = 0; i < count && i < kMaxPortals; ++i) {
    const std::optional<GateInfo> gate = inspectGate(core, i);
    if (!gate) {
      reply += lucent::format("\ngate={} INVALID or unloaded", i);
      continue;
    }
    reply += lucent::format("\ngate={} targetLevel={} pathMoby={} firstPoint=({}, {}, {}) nodes={}",
                            i,
                            gate->levelId,
                            gate->pathMoby,
                            gate->firstPoint.x,
                            gate->firstPoint.y,
                            gate->firstPoint.z,
                            gate->nodeCount);
    for (uint32_t node = 0; node < gate->nodeCount; ++node) {
      reply += lucent::format("\n  node={} position=({}, {}, {})",
                              node,
                              gate->nodes[node].x,
                              gate->nodes[node].y,
                              gate->nodes[node].z);
    }
  }
  return reply;
}

} // namespace spyro1::teleport
