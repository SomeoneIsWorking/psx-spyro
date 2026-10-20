#include "spyro1/core/frame/ordering_table.h"

#include "core.h"
#include "core/native_execution.h"
#include "execution_control.h"
#include "native_dispatch.h"
#include "render/terrain/guest_depth_bins.h"

namespace spyro1 {
namespace {

void flattenOverride(Core *core) {
  OrderingTable::assignSlots(*core, core->r[4]);
  (void)psx::cpu::callOriginalOrPropagate(*core, OrderingTable::kFlatten);
}

} // namespace

void OrderingTable::assignSlots(Core &core, std::uint32_t bins) {
  namespace depth_bins = spyro::depth_bins;
  depth_bins::assignBins(core,
                         {.table = depth_bins::kTable,
                          .base = core.mem_r32(kBinTablePointer),
                          .named = bins,
                          .linked = bins});
  depth_bins::assignBins(
      core,
      {.table = kOverlayTable, .base = core.mem_r32(kOverlayPairPointer), .named = 1, .linked = 1});
}

void OrderingTable::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kFlatten, "spyro1-ordering-table", flattenOverride);
}

} // namespace spyro1
