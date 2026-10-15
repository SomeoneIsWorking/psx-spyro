#include "spyro1_actor_between.h"

#include "blend.h"
#include "core.h"
#include "gp0_primitive_decode.h"
#include "gte_registers.h"
#include "guest_depth_bins.h"
#include "guest_packet_chain.h"
#include "lucent/log.h"
#include "spyro1_actor_facts.h"
#include "spyro1_terrain_facts.h"

#include <cstdlib>
#include <span>

namespace spyro1::actors {
namespace {

using terrain::kRenderGlobals;

constexpr std::uint32_t kCameraBytes = 0x34u;

} // namespace

std::uint32_t wordBetween(std::uint32_t from, std::uint32_t to, float t) {
  return static_cast<std::uint32_t>(
      psx::present::lerpInt(static_cast<std::int32_t>(from), static_cast<std::int32_t>(to), t));
}

std::uint8_t angleBetween(std::uint8_t from, std::uint8_t to, float t) {
  const auto turn = static_cast<std::int8_t>(static_cast<std::uint8_t>(to - from));
  return static_cast<std::uint8_t>(from + psx::present::lerpInt(0, turn, t));
}

MobyBytes recordBetween(const MobyBytes &from, const MobyBytes &to, float t) {
  MobyBytes shown = to;
  const bool sameMesh =
      from[moby::kMesh] == to[moby::kMesh] && from[moby::kMesh + 1] == to[moby::kMesh + 1];
  const bool sameSpace =
      ((from[moby::kCullDistance] ^ to[moby::kCullDistance]) & moby::kScreenSpace) == 0;
  if (!sameMesh || !sameSpace) {
    return shown;
  }
  for (const std::uint32_t at : {moby::kX, moby::kY, moby::kZ}) {
    storeWord(shown, at, wordBetween(loadWord(from, at), loadWord(to, at), t));
  }
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    shown[moby::kRotation + axis] =
        angleBetween(from[moby::kRotation + axis], to[moby::kRotation + axis], t);
  }
  return shown;
}

void emitDepthBins(const spyro::guest_memory::WorkingMemory &memory,
                   const spyro::guest_memory::PacketArena &arena,
                   std::uint32_t table,
                   psx::present::PrimitiveSink &sink,
                   const char *who) {
  constexpr std::uint32_t kBins = terrain::kOrderingTableBytes / spyro::depth_bins::kBinStride;
  spyro::guest_memory::PacketChains chains(memory, arena);
  for (std::uint32_t bin = kBins; bin-- > 0;) {
    const std::uint32_t first = memory.r32(table + bin * spyro::depth_bins::kBinStride +
                                           spyro::depth_bins::kBinFirstOffset);
    spyro::guest_memory::DrawMode drawMode;
    chains.walk(first, [&](std::uint32_t packet, std::span<const std::uint32_t> words) {
      if (drawMode.take(words)) {
        return;
      }
      auto primitive = psx::gpu::decodePacketPrimitive(words);
      if (!primitive) {
        lucent::error(who, "the packet 0x{:08X} at 0x{:08X} is not a primitive", words[0], packet);
        std::abort();
      }
      drawMode.apply(*primitive);
      sink.emit(psx::present::OtSlot{spyro::depth_bins::kTable, bin}, *primitive);
    });
  }
}

std::array<spyro::guest_memory::GuestRange, 3> binnedWorkingRanges(Core &core) {
  const std::uint32_t cursor = core.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = core.mem_r32(terrain::kPacketPoolEnd);
  return {{
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {core.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {kRenderGlobals.primitiveCursor, 4},
  }};
}

void readBytes(Core &core, std::uint32_t base, std::span<std::uint8_t> bytes) {
  for (std::uint32_t at = 0; at < bytes.size(); ++at) {
    bytes[at] = core.mem_r8(base + at);
  }
}

void mapBytes(spyro::guest_memory::HostMemory &host,
              std::uint32_t base,
              std::span<const std::uint8_t> bytes) {
  host.mapEmpty(base, static_cast<std::uint32_t>(bytes.size()));
  for (std::uint32_t at = 0; at < bytes.size(); ++at) {
    host.w8(base + at, bytes[at]);
  }
}

void mapCamera(spyro::guest_memory::HostMemory &host,
               const spyro::guest_terrain::InBetweenCamera &camera) {
  host.mapEmpty(kRenderGlobals.cameraRotation, kCameraBytes);
  for (std::uint32_t i = 0; i < psx::gte::kRotationWords; ++i) {
    host.w32(kRenderGlobals.cameraRotation + i * 4, camera.rotation[i]);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    host.w32(kRenderGlobals.cameraPosition + i * 4, camera.position[i]);
  }
}

std::uint32_t mapBinnedWorkspace(spyro::guest_memory::HostMemory &host,
                                 const spyro::guest_memory::PacketArena &arena,
                                 const spyro::guest_terrain::InBetweenCamera &camera) {
  host.mapEmpty(kVertexScreens, kScratchpadBytes);
  host.mapEmpty(kWorkBlock, kWorkBlockBytes);
  host.mapEmpty(arena.base, arena.bytes);
  mapCamera(host, camera);
  host.map(kRenderGlobals.orderingTable, 4);
  const std::uint32_t table = host.r32(kRenderGlobals.orderingTable);
  host.mapEmpty(table, terrain::kOrderingTableBytes);
  host.mapEmpty(kRenderGlobals.primitiveCursor, 4);
  host.w32(kRenderGlobals.primitiveCursor, arena.base);
  return table;
}

} // namespace spyro1::actors
