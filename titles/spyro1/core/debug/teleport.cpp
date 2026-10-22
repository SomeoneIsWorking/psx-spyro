#include "spyro1/core/debug/teleport.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "core/debug/moby_teleport.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <lucent/log.h>
#include <optional>
#include <vector>

namespace spyro1::teleport {
namespace {

constexpr uint32_t kRamBegin = 0x80000000u;
constexpr uint32_t kRamEnd = 0x801fffffu;
using spyro1::guest::kGamestate;
using spyro1::guest::kLevelId;
using spyro1::guest::kLevelMobys;
using spyro1::guest::kLoadStage;
using spyro1::guest::kMobyClassCrystalDragon;
using spyro1::guest::kMobyProps;
using spyro1::guest::kMobyStride;
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

bool validAddress(uint32_t address, uint32_t bytes = 4u) {
  return address >= kRamBegin && address <= kRamEnd && bytes <= kRamEnd - address + 1u;
}

bool validPointer(uint32_t address, uint32_t bytes = 4u) {
  return address != 0u && validAddress(address, bytes);
}

bool readPointer(Core &core, uint32_t address, uint32_t &out) {
  if (!validAddress(address)) {
    return false;
  }
  out = core.mem_r32(address);
  return validPointer(out);
}

Position readPosition(Core &core, uint32_t address) {
  return {static_cast<int32_t>(core.mem_r32(address)),
          static_cast<int32_t>(core.mem_r32(address + 4u)),
          static_cast<int32_t>(core.mem_r32(address + 8u))};
}

std::optional<GateInfo> readGatePath(Core &core, uint32_t portal) {
  if (!validPointer(portal) || !validAddress(portal + kPortalFirstPoint, 12u)) {
    return std::nullopt;
  }
  const int32_t pathMoby = static_cast<int32_t>(core.mem_r32(portal + kPortalPathMoby));
  const uint32_t levelMobys = core.mem_r32(kLevelMobys);
  if (pathMoby < 0 || !validPointer(levelMobys)) {
    return std::nullopt;
  }
  const uint64_t mobyOffset = static_cast<uint64_t>(static_cast<uint32_t>(pathMoby)) * kMobyStride;
  if (mobyOffset > 0xffffffffu) {
    return std::nullopt;
  }
  const uint32_t moby = levelMobys + static_cast<uint32_t>(mobyOffset);
  if (!validAddress(moby, 4u)) {
    return std::nullopt;
  }
  const uint32_t props = core.mem_r32(moby + kMobyProps);
  uint32_t path = 0;
  if (!validPointer(props) || !readPointer(core, props + kPathData, path)) {
    return std::nullopt;
  }
  const uint32_t nodeCount = core.mem_r8(path + kPathNodeCount);
  if (nodeCount == 0u || nodeCount > kMaxPathNodes ||
      !validAddress(path + kPathNodes, nodeCount * kPathNodeStride)) {
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
  if (index >= count || index >= kMaxPortals || !validAddress(kPortals + index * 4u)) {
    return std::nullopt;
  }
  uint32_t portal = 0;
  if (!readPointer(core, kPortals + index * 4u, portal)) {
    return std::nullopt;
  }
  return readGatePath(core, portal);
}

bool inActiveField(Core &core) {
  return core.mem_r32(kGamestate) == 0u && core.mem_r32(kLoadStage) == 0xffffffffu;
}

void placeSpyro(Core &core, const Position &destination) {
  for (const uint32_t field : {kSpyroPreviousPosition, kSpyroPosition}) {
    core.mem_w32(kSpyro + field, static_cast<uint32_t>(destination.x));
    core.mem_w32(kSpyro + field + 4u, static_cast<uint32_t>(destination.y));
    core.mem_w32(kSpyro + field + 8u, static_cast<uint32_t>(destination.z));
  }
}

// SCUS_942.28's g_LevelMobys, its active field and Spyro's two position copies.
class MobyTeleport final : public spyro::MobyTeleport {
protected:
  [[nodiscard]] uint32_t levelMobysWord() const override {
    return kLevelMobys;
  }
  [[nodiscard]] bool inActiveField(Core &core) const override {
    return teleport::inActiveField(core);
  }
  void placeSpyro(Core &core, const Position &destination) const override {
    teleport::placeSpyro(core, destination);
  }
};

bool teleportToGate(Core &core, uint32_t index, uint32_t node) {
  if (!inActiveField(core)) {
    lucent::info("spyro-teleport", "gate-teleport refused: gameplay is not in an active field");
    return false;
  }
  const std::optional<GateInfo> gate = inspectGate(core, index);
  if (!gate) {
    lucent::info(
        "spyro-teleport", "gate-teleport refused: gate {} is not a valid loaded portal", index);
    return false;
  }
  if (node >= gate->nodeCount) {
    lucent::info(
        "spyro-teleport", "gate-teleport refused: node {} is outside gate {}", node, index);
    return false;
  }
  const Position destination = gate->nodes[node];
  placeSpyro(core, destination);
  lucent::info("spyro-teleport",
               "gate-teleport gate={} node={} targetLevel={} position=({}, {}, {})",
               index,
               node,
               gate->levelId,
               destination.x,
               destination.y,
               destination.z);
  return true;
}

void listGates(Core &core, std::FILE *reply) {
  const uint32_t count = core.mem_r32(kPortalCount);
  std::fprintf(reply,
               "level=%d nextLevel=%d gamestate=%u loadStage=%u portals=%u\n",
               static_cast<int32_t>(core.mem_r32(kLevelId)),
               static_cast<int32_t>(core.mem_r32(kNextLevelId)),
               core.mem_r32(kGamestate),
               core.mem_r32(kLoadStage),
               count);
  for (uint32_t i = 0; i < count && i < kMaxPortals; ++i) {
    const std::optional<GateInfo> gate = inspectGate(core, i);
    if (!gate) {
      std::fprintf(reply, "gate=%u INVALID or unloaded\n", i);
      continue;
    }
    std::fprintf(reply,
                 "gate=%u targetLevel=%d pathMoby=%d firstPoint=(%d, %d, %d) nodes=%u\n",
                 i,
                 gate->levelId,
                 gate->pathMoby,
                 gate->firstPoint.x,
                 gate->firstPoint.y,
                 gate->firstPoint.z,
                 gate->nodeCount);
    for (uint32_t node = 0; node < gate->nodeCount; ++node) {
      std::fprintf(reply,
                   "  node=%u position=(%d, %d, %d)\n",
                   node,
                   gate->nodes[node].x,
                   gate->nodes[node].y,
                   gate->nodes[node].z);
    }
  }
}

} // namespace

bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  const char *name = input.command;
  if (std::strcmp(name, "gates") == 0) {
    listGates(core, reply);
    return true;
  }
  const MobyTeleport mobys;
  if (std::strcmp(name, "dragons") == 0) {
    mobys.list(core, kMobyClassCrystalDragon, reply);
    return true;
  }
  if (std::strcmp(name, "gate-teleport") == 0) {
    const std::optional<std::uint32_t> index = input.args.number(1);
    const std::optional<std::uint32_t> node = input.args.number(2);
    if (!index || !node) {
      std::fprintf(reply, "usage: gate-teleport <portal-index> <path-node>\n");
      return true;
    }
    std::fprintf(reply,
                 "gate-teleport %u %u: %s\n",
                 *index,
                 *node,
                 teleportToGate(core, *index, *node) ? "ok" : "refused");
    return true;
  }
  if (std::strcmp(name, "dragon-teleport") == 0) {
    const std::optional<std::uint32_t> index = input.args.number(1);
    if (!index) {
      std::fprintf(reply, "usage: dragon-teleport <dragon-index>\n");
      return true;
    }
    std::fprintf(reply,
                 "dragon-teleport %u: %s\n",
                 *index,
                 mobys.teleport(core, kMobyClassCrystalDragon, *index) ? "ok" : "refused");
    return true;
  }
  return mobys.command(core, input, reply);
}

} // namespace spyro1::teleport
