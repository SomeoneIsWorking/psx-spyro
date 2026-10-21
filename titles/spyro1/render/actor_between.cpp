#include "spyro1/render/actor_between.h"

#include "core.h"
#include "guest_memory_map.h"
#include "packet_chains.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

namespace spyro1::actors {
namespace {

using terrain::kRenderGlobals;

} // namespace

MobyBytes recordBetween(const MobyBytes &from, const MobyBytes &to, float t) {
  MobyBytes shown = to;
  const bool sameMesh = load<std::uint16_t>(from, moby::kMesh) == load<std::uint16_t>(to, moby::kMesh);
  const bool sameSpace = ((from[moby::kCullDistance] ^ to[moby::kCullDistance]) & moby::kScreenSpace) == 0;
  if (!sameMesh || !sameSpace) {
    return shown;
  }
  for (const std::uint32_t at : {moby::kX, moby::kY, moby::kZ}) {
    wordBetween(shown, from, to, at, t);
  }
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    shown[moby::kRotation + axis] = angleBetween(from[moby::kRotation + axis], to[moby::kRotation + axis], t);
  }
  return shown;
}

std::array<GuestSpan, 3> binnedWorkingRanges(Core &core) {
  const std::uint32_t cursor = core.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = core.mem_r32(terrain::kPacketPoolEnd);
  return {{
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {core.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {kRenderGlobals.primitiveCursor, 4},
  }};
}

InBetween::InBetween(Core &core, std::span<const psx::present::LevelWord> reads)
    : gte_(core.gte()), memory_(core, host_), arena_(terrain::kTerrainImage.windows(core).inBetweenArena) {
  host_.overlay(reads);
  host_.zero(arena_.address, arena_.bytes);
  provideWord(kRenderGlobals.primitiveCursor, arena_.address);
  provideWord(terrain::kPacketPoolEnd, arena_.address + arena_.bytes);
}

void InBetween::provide(std::uint32_t address, std::span<const std::uint8_t> bytes) {
  host_.provide(address, std::as_bytes(bytes));
}

void InBetween::provideWord(std::uint32_t address, std::uint32_t word) {
  host_.provide(address, std::as_bytes(std::span(&word, 1)));
}

void InBetween::mapWorkspace() {
  host_.zero(kVertexScreens, psx::kScratchpadBytes);
  host_.zero(kWorkBlock, kWorkBlockBytes);
}

void InBetween::mapCamera(const spyro::guest_terrain::InBetweenCamera &camera) {
  host_.provide(kRenderGlobals.cameraRotation, std::as_bytes(std::span(camera.rotation)));
  host_.provide(terrain::kSharedFacts.classify.viewRotation, std::as_bytes(std::span(camera.viewRotation)));
  host_.provide(kRenderGlobals.cameraPosition, std::as_bytes(std::span(camera.position)));
  host_.provide(kRenderGlobals.cameraAngles, std::as_bytes(std::span(camera.angles)));
}

void InBetween::mapTable() {
  table_ = memory_.mem_r32(kRenderGlobals.orderingTable);
  host_.zero(table_, terrain::kOrderingTableBytes);
}

void InBetween::emitBins(psx::present::PrimitiveSink &sink) const {
  psx::present::PacketChainWalker walker(memory_, psx::present::PacketBound::within(arena_));
  psx::present::DrawModeTracker drawMode;
  walker.emitBuckets({.base = table_,
                      .buckets = terrain::kOrderingTableBytes / spyro::depth_bins::kBinStride,
                      .first = {spyro::depth_bins::kTable, 0},
                      .stride = spyro::depth_bins::kBinStride,
                      .headOffset = spyro::depth_bins::kBinFirstOffset},
                     drawMode,
                     sink);
}

void InBetween::emitChain(std::uint32_t first,
                          psx::present::OtSlot slot,
                          psx::present::DrawModeTracker &drawMode,
                          psx::present::PrimitiveSink &sink) const {
  psx::present::PacketChainWalker(memory_, psx::present::PacketBound::within(arena_))
      .emitChain(first, slot, drawMode, sink);
}

} // namespace spyro1::actors
