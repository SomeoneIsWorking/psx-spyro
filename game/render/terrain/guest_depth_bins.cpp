#include "render/terrain/guest_depth_bins.h"

#include "render/field/guest_working_memory.h"

#include "core.h"
#include "draw_list_cycle.h"
#include "lucent/log.h"
#include "ordering_table.h"

#include <algorithm>
#include <optional>

namespace spyro::depth_bins {
namespace {

constexpr std::uint32_t kMarkSlack = kBinStride;

// Assigns `slot` to the chain of the (head, first) pair at `pair`, first-linked packet to head.
void assignPair(Core &core, std::uint32_t pair, psx::present::OtSlot slot, Claim claim) {
  const std::uint32_t head = core.mem_r32(pair);
  std::uint32_t packet = core.mem_r32(pair + kBinFirstOffset);
  psx::gpu::LinkCycleCheck cycle;
  while (packet != 0) {
    if (!cycle.enter(psx::gpu::mainRamOffsetOf(packet))) {
      psx::gpu::refuseDrawListCycle(
          core,
          packet,
          "render/terrain/guest_depth_bins.cpp:assignPair",
          lucent::format("the chain of bin pair 0x{:08X} (slot {}:{}) returns to it",
                         pair,
                         slot.table,
                         slot.index));
      return;
    }
    core.otTables.assign(packet, slot);
    if (packet < claim.below) {
      const std::optional<psx::present::RecordKey> key = core.emission.keyFor(packet);
      if (!key || key->producer == core.emission.current().producer) {
        core.emission.bindPacket(packet);
      }
    }
    // Until the flatten links them, a bin's chain runs from its first packet to the head and ends
    // in a zero link.
    const psx::gpu::OtNode tag(psx::gpu::mainRamOffsetOf(packet),
                               psx::gpu::OtHeaderWord{core.mem_r32(packet)});
    packet = packet == head || tag.endsChain() ? 0 : psx::gpu::guestAddressOf(tag.nextRamOffset());
  }
}

} // namespace

Bins publishedBins(Core &core, const guest_render_globals::Globals &globals) {
  const std::uint32_t table = core.mem_r32(globals.orderingTable);
  const std::int32_t reach = static_cast<std::int32_t>(
      (core.mem_r32(globals.orderingTableMark) + kMarkSlack - table) / kBinStride);
  return Bins{.table = kTable,
              .base = table,
              .named = kBins,
              .linked = static_cast<std::uint32_t>(
                  std::max(0, std::min(static_cast<std::int32_t>(kBins), reach)))};
}

Bins overlayBins(Core &core, const guest_render_globals::Globals &globals) {
  return Bins{
      .table = kOverlayTable, .base = core.mem_r32(globals.overlayPair), .named = 1, .linked = 1};
}

void assignBins(Core &core, const Bins &bins, Claim claim) {
  core.otTables.name(bins.table, bins.base, bins.named, kBinStride, psx::gpu::OtWalk::HighToLow);
  for (std::uint32_t bin = 0; bin < bins.linked; ++bin) {
    assignPair(core, bins.base + bin * kBinStride, psx::present::OtSlot{bins.table, bin}, claim);
  }
}

void assignOverlay(Core &core, const Bins &published, const Bins &overlay, Claim claim) {
  assignBins(core, overlay, claim);
  if (core.mem_r32(overlay.base + kBinFirstOffset) != 0) {
    return;
  }
  // The flatten walks the bins deepest first, so the lowest linked one ends the chain.
  for (std::uint32_t bin = 0; bin < published.linked; ++bin) {
    if (const std::uint32_t head = core.mem_r32(published.base + bin * kBinStride); head != 0) {
      core.otTables.placeEmpty(psx::present::OtSlot{overlay.table, 0}, head);
      return;
    }
  }
}

std::uint32_t linkIntoBin(const psx::present::EmitMemory &memory,
                          std::uint32_t slot,
                          std::uint32_t first,
                          std::uint32_t last) {
  const std::uint32_t head = memory.mem_r32(slot);
  memory.mem_w32(slot, last);
  if (head == 0) {
    memory.mem_w32(slot + kBinFirstOffset, first);
  } else {
    guest_memory::linkAfter(memory, head, first);
  }
  return head;
}

} // namespace spyro::depth_bins
