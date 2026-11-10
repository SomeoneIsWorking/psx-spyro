#include "render/terrain/engine_terrain_image.h"

#include "core.h"
#include "core/native_execution.h"
#include "gte_registers.h"
#include "render/terrain/guest_terrain_passes.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

} // namespace

std::uint32_t EngineTerrainImage::scratch(Core &core) const {
  return core.mem_r32(globals_.scratchBaseWord) - facts_.scratchListsBelowEnd;
}

void EngineTerrainImage::drawPasses(TerrainFrame &frame) const {
  Core &core = frame.core;
  core.gte().writeData(gte::kVxy1, frame.scratch);
  // The drawer's one call out of its own body.
  {
    spyro::PreservedReturnAddress returnAddress(core);
    psx::cpu::callGuestNow(
        core,
        facts_.overrideName,
        psx::cpu::GuestCallSite::fromJal(facts_.visibilityCallSite, facts_.sectorVisibility),
        0u);
  }
  core.mem_w32(facts_.visibleSectorCount, core.r[2]);
  classifySectors(frame);
  drawDetailSectors(frame);
  drawTranslucentSectors(frame);
  // The translucent pass ends both split lists (guest_terrain_translucent.cpp).
  splitCoarsePolygons(frame);
  splitFinePolygons(frame);
  resplitOversizedPrimitives(frame);
  drawFarSectors(frame);
}

} // namespace spyro::guest_terrain
