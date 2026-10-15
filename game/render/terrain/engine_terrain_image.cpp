#include "engine_terrain_image.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_passes.h"
#include "native_execution.h"
#include "spyro_session.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

} // namespace

FieldWindows EngineTerrainImage::windows(Core &core) const {
  // The windows are disjoint: the guest's own cursor can already be past the field's packets.
  const std::uint32_t scratch =
      core.mem_r32(globals_.scratchBaseWord) - facts_.scratchListsBelowEnd;
  const std::uint32_t tableBase = core.mem_r32(globals_.orderingTable);
  // The scratch block runs to `frameTop` because a split list can grow past retail's block when
  // the camera sits between two fields.
  const std::uint32_t frameTop = std::max({scratch + facts_.scratchListsBelowEnd,
                                           tableBase + kOrderingWindowBytes,
                                           facts_.foggedColours + kFoggedColourBytes});
  return FieldWindows{
      .scratch = scratch,
      .tableBase = tableBase,
      .arenaEnd = scratch + kPrimitiveArenaBytes,
      .inBetweenArena = guest_memory::packetArenaWindow(frameTop),
      .owned = {guest_memory::GuestRange{kScratchpad, kScratchpadBytes},
                guest_memory::GuestRange{scratch, frameTop - scratch},
                guest_memory::GuestRange{scratch, kFlagWindowBytes},
                guest_memory::GuestRange{tableBase, kOrderingWindowBytes},
                guest_memory::GuestRange{facts_.foggedColours, kFoggedColourBytes}}};
}

guest_camera::Matrices EngineTerrainImage::camera(Core &core,
                                                  const guest_camera::Angles &angles) const {
  return guest_camera::EngineBuilder(
             core, {.sine = globals_.cameraSineTable, .cosine = globals_.cameraCosineTable})
      .build(angles);
}

void EngineTerrainImage::drawPasses(TerrainFrame &frame) const {
  Core &core = frame.core;
  core.gte().writeData(gte::kVxy1, frame.scratch);
  if (frame.realField()) {
    const std::uint32_t cursor = core.mem_r32(globals_.primitiveCursor);
    spyro::session(core).terrainPacketBudget =
        static_cast<std::int32_t>(frame.scratch + kPrimitiveArenaBytes - cursor);
    // The drawer's one call out of its own body. Only the guest can answer it, so an in-between
    // field is handed the sectors its visibility was decided for, not asked again.
    {
      spyro::PreservedReturnAddress returnAddress(core);
      spyro::callGuestJumpedFrom(
          core,
          facts_.overrideName,
          {.jal = facts_.visibilityCallSite, .target = facts_.sectorVisibility},
          0u);
    }
    core.mem_w32(facts_.visibleSectorCount, core.r[2]);
    // The guest's answer, copied now: the classification clears each byte as it consumes it.
    const std::uint32_t count = core.mem_r32(facts_.classify.sectorCount);
    std::vector<std::uint8_t> visible(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      visible[i] = frame.memory.r8(kScratchpad + i);
    }
    spyro::session(core).terrainVisibility = std::move(visible);
  }
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
