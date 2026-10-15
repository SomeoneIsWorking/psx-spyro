#include "spyro1_ordering_table.h"

#include "core.h"
#include "execution_control.h"
#include "guest_depth_bins.h"
#include "native_dispatch.h"
#include "native_execution.h"

namespace spyro1 {
namespace {

void flattenOverride(Core *core) {
  OrderingTable::assignSlots(*core, core->r[4]);
  (void)psx::cpu::completeOrPropagate(
      *core,
      psx::cpu::callOriginal(
          *core, OrderingTable::kFlatten, psx::cpu::ExecutionBudget::currentTurn(*core)));
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
