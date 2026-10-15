#include "spyro_gate_debug.h"
#include "guest_globals.h"

#include "core.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <lucent/log.h>
#include <optional>
#include <string_view>

namespace spyro::gate_debug {
namespace {

constexpr uint32_t kRamBegin = 0x80000000u;
constexpr uint32_t kRamEnd = 0x801fffffu;
using spyro::guest::kGamestate;
using spyro::guest::kLevelId;
using spyro::guest::kLevelMobys;
using spyro::guest::kLoadStage;
using spyro::guest::kNextLevelId;
using spyro::guest::kPortalCount;
using spyro::guest::kPortals;
using spyro::guest::kSpyro;

constexpr uint32_t kPortalPathMoby = 0x18u;
constexpr uint32_t kPortalLevelId = 0x1cu;
constexpr uint32_t kPortalFirstPoint = 0x20u;
constexpr uint32_t kMobyStride = 0x58u;
constexpr uint32_t kMobyProps = 0x00u;
constexpr uint32_t kPathData = 0x00u;
constexpr uint32_t kPathNodeCount = 0x00u;
constexpr uint32_t kPathNodes = 0x08u;
constexpr uint32_t kPathNodeStride = 0x10u;
constexpr uint32_t kSpyroPosition = 0x00u;
constexpr uint32_t kSpyroPreviousPosition = 0x8cu;

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

bool readGatePath(Core &core, uint32_t portal, GateInfo &out) {
  if (!validPointer(portal) || !validAddress(portal + kPortalFirstPoint, 12u)) {
    return false;
  }
  const int32_t pathMoby = static_cast<int32_t>(core.mem_r32(portal + kPortalPathMoby));
  const uint32_t levelMobys = core.mem_r32(kLevelMobys);
  if (pathMoby < 0 || !validPointer(levelMobys)) {
    return false;
  }
  const uint64_t mobyOffset = static_cast<uint64_t>(static_cast<uint32_t>(pathMoby)) * kMobyStride;
  if (mobyOffset > 0xffffffffu) {
    return false;
  }
  const uint32_t moby = levelMobys + static_cast<uint32_t>(mobyOffset);
  if (!validAddress(moby, 4u)) {
    return false;
  }
  const uint32_t props = core.mem_r32(moby + kMobyProps);
  uint32_t path = 0;
  if (!validPointer(props) || !readPointer(core, props + kPathData, path)) {
    return false;
  }
  const uint32_t nodeCount = core.mem_r8(path + kPathNodeCount);
  if (nodeCount == 0u || nodeCount > 16u ||
      !validAddress(path + kPathNodes, nodeCount * kPathNodeStride)) {
    return false;
  }

  out.valid = true;
  out.portalAddress = portal;
  out.pathMoby = pathMoby;
  out.levelId = static_cast<int32_t>(core.mem_r32(portal + kPortalLevelId));
  out.firstPoint = readPosition(core, portal + kPortalFirstPoint);
  out.nodeCount = nodeCount;
  for (uint32_t i = 0; i < nodeCount; ++i) {
    out.nodes[i] = readPosition(core, path + kPathNodes + i * kPathNodeStride);
  }
  return true;
}

struct GateTarget {
  uint32_t index = 0;
  uint32_t node = 0;
};

std::optional<uint32_t> takeNumber(std::string_view &rest) {
  const std::size_t start = rest.find_first_not_of(" \t");
  if (start == std::string_view::npos) {
    return std::nullopt;
  }
  rest.remove_prefix(start);
  uint32_t value = 0;
  const auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
  if (error != std::errc{}) {
    return std::nullopt;
  }
  rest.remove_prefix(static_cast<std::size_t>(end - rest.data()));
  return value;
}

// "gate-teleport <portal-index> <path-node>"
std::optional<GateTarget> parseTarget(std::string_view line) {
  const std::size_t space = line.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view rest = line.substr(space);
  const std::optional<uint32_t> index = takeNumber(rest);
  const std::optional<uint32_t> node = takeNumber(rest);
  if (!index || !node) {
    return std::nullopt;
  }
  return GateTarget{.index = *index, .node = *node};
}

} // namespace

bool inspectGate(Core &core, uint32_t index, GateInfo &out) {
  out = {};
  const uint32_t count = core.mem_r32(kPortalCount);
  if (index >= count || index >= 16u || !validAddress(kPortals + index * 4u)) {
    return false;
  }
  uint32_t portal = 0;
  if (!readPointer(core, kPortals + index * 4u, portal)) {
    return false;
  }
  return readGatePath(core, portal, out);
}

bool teleportToGate(Core &core, uint32_t index, uint32_t node) {
  if (core.mem_r32(kGamestate) != 0u || core.mem_r32(kLoadStage) != 0xffffffffu) {
    lucent::info("spyro-gate", "gate-teleport refused: gameplay is not in an active field");
    return false;
  }
  GateInfo gate;
  if (!inspectGate(core, index, gate)) {
    lucent::info(
        "spyro-gate", "gate-teleport refused: gate {} is not a valid loaded portal", index);
    return false;
  }
  if (node >= gate.nodeCount || !validAddress(kSpyro + kSpyroPreviousPosition, 12u)) {
    lucent::info("spyro-gate", "gate-teleport refused: node {} is outside gate {}", node, index);
    return false;
  }
  const Position destination = gate.nodes[node];
  core.mem_w32(kSpyro + kSpyroPreviousPosition, static_cast<uint32_t>(destination.x));
  core.mem_w32(kSpyro + kSpyroPreviousPosition + 4u, static_cast<uint32_t>(destination.y));
  core.mem_w32(kSpyro + kSpyroPreviousPosition + 8u, static_cast<uint32_t>(destination.z));
  core.mem_w32(kSpyro + kSpyroPosition, static_cast<uint32_t>(destination.x));
  core.mem_w32(kSpyro + kSpyroPosition + 4u, static_cast<uint32_t>(destination.y));
  core.mem_w32(kSpyro + kSpyroPosition + 8u, static_cast<uint32_t>(destination.z));
  lucent::info("spyro-gate",
               "gate-teleport gate={} node={} targetLevel={} position=({}, {}, {})",
               index,
               node,
               gate.levelId,
               destination.x,
               destination.y,
               destination.z);
  return true;
}

bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  const auto [name, line] = input;
  if (std::strcmp(name, "gates") != 0 && std::strcmp(name, "gate-teleport") != 0) {
    return false;
  }
  if (std::strcmp(name, "gate-teleport") == 0) {
    const std::optional<GateTarget> target = parseTarget(line != nullptr ? line : "");
    if (!target) {
      std::fprintf(reply, "usage: gate-teleport <portal-index> <path-node>\n");
      return true;
    }
    std::fprintf(reply,
                 "gate-teleport %u %u: %s\n",
                 target->index,
                 target->node,
                 teleportToGate(core, target->index, target->node) ? "ok" : "refused");
    return true;
  }

  const uint32_t count = core.mem_r32(kPortalCount);
  std::fprintf(reply,
               "level=%d nextLevel=%d gamestate=%u loadStage=%u portals=%u\n",
               static_cast<int32_t>(core.mem_r32(kLevelId)),
               static_cast<int32_t>(core.mem_r32(kNextLevelId)),
               core.mem_r32(kGamestate),
               core.mem_r32(kLoadStage),
               count);
  for (uint32_t i = 0; i < count && i < 16u; ++i) {
    GateInfo gate;
    if (!inspectGate(core, i, gate)) {
      std::fprintf(reply, "gate=%u INVALID or unloaded\n", i);
      continue;
    }
    std::fprintf(reply,
                 "gate=%u targetLevel=%d pathMoby=%d firstPoint=(%d, %d, %d) nodes=%u\n",
                 i,
                 gate.levelId,
                 gate.pathMoby,
                 gate.firstPoint.x,
                 gate.firstPoint.y,
                 gate.firstPoint.z,
                 gate.nodeCount);
    for (uint32_t node = 0; node < gate.nodeCount; ++node) {
      std::fprintf(reply,
                   "  node=%u position=(%d, %d, %d)\n",
                   node,
                   gate.nodes[node].x,
                   gate.nodes[node].y,
                   gate.nodes[node].z);
    }
  }
  return true;
}

} // namespace spyro::gate_debug
