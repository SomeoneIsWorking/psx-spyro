#include "spyro1/render/ordering_table.h"

#include "core.h"
#include "native_dispatch.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"

namespace spyro1 {
namespace {

void flattenOverride(Core *core) {
  OrderingTable::assignSlots(*core, core->r[4]);
  (void)psx::cpu::callOriginalOrPropagate(*core, OrderingTable::kFlatten);
}

} // namespace

void OrderingTable::assignSlots(Core &core, std::uint32_t bins) {
  namespace depth_bins = spyro::depth_bins;
  const depth_bins::Bins published{.table = depth_bins::kTable,
                                   .base = core.mem_r32(terrain::kRenderGlobals.orderingTable),
                                   .named = bins,
                                   .linked = bins};
  depth_bins::assignBins(core, published);
  depth_bins::assignOverlay(
      core, published, depth_bins::overlayBins(core, terrain::kRenderGlobals));
}

void OrderingTable::registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kFlatten, "spyro1-ordering-table", flattenOverride);
}

} // namespace spyro1
