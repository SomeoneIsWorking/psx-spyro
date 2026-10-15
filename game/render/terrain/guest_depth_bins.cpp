#include "guest_depth_bins.h"

#include "core.h"
#include "lucent/log.h"
#include "ordering_table.h"

#include <algorithm>
#include <cstdlib>

namespace spyro::depth_bins {
namespace {

// The bin count both draws ask the flatten for (0x580); its start clamps to the mark's bin.
constexpr std::int32_t kScanBins = 0x580;
constexpr std::uint32_t kMarkSlack = kBinStride;

// A packet's tag: GP0 word count in the high byte, the next packet's RAM offset in the low 24
// bits, zero ending a bin's chain until the flatten links it. A bin's chain starts at the second
// word of its pair (the first packet linked) and ends at the head.
constexpr std::uint32_t kNextMask = 0x00FFFFFFu;

// Assigns `slot` to the chain of the (head, first) pair at `pair`, first-linked packet to head.
void assignPair(Core &core, std::uint32_t pair, psx::present::OtSlot slot) {
  const std::uint32_t head = core.mem_r32(pair);
  std::uint32_t packet = core.mem_r32(pair + kBinFirstOffset);
  for (int assigned = 0; packet != 0; ++assigned) {
    if (assigned == psx::gpu::kOtNodeLimit) {
      lucent::error("depthbins",
                    "refusing to assign more than {} packets to slot {}:{}: the chain at 0x{:08X} "
                    "runs on past the packet pool",
                    psx::gpu::kOtNodeLimit,
                    slot.table,
                    slot.index,
                    pair);
      std::abort();
    }
    core.otTables.assign(packet, slot);
    const std::uint32_t next = core.mem_r32(packet) & kNextMask;
    packet = packet == head || next == 0 ? 0 : psx::gpu::guestAddressOf(next);
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
              .linked = static_cast<std::uint32_t>(std::max(0, std::min(kScanBins, reach)))};
}

void assignBins(Core &core, const Bins &bins) {
  core.otTables.name(bins.table, bins.base, bins.named, kBinStride, psx::gpu::OtWalk::HighToLow);
  for (std::uint32_t bin = 0; bin < bins.linked; ++bin) {
    assignPair(core, bins.base + bin * kBinStride, psx::present::OtSlot{bins.table, bin});
  }
}

} // namespace spyro::depth_bins
