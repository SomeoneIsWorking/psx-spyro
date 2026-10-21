#include "spyro1/render/terrain_image.h"

#include "core.h"
#include "core/session.h"
#include "gte_registers.h"
#include "native_dispatch.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_terrain_passes.h"
#include "render/terrain/terrain_state_producer.h"
#include "spyro1/render/terrain_passes.h"

#include <memory>

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
using spyro::guest_terrain::FieldWindows;

// The flag bytes, the sector lists and the re-split queue, which grows from kFarList.
constexpr std::uint32_t kScratchBlockBytes = 0x3000u;

} // namespace

FieldWindows TerrainImage::windows(Core &core) const {
  const std::uint32_t tableBase = core.mem_r32(kRenderGlobals.orderingTable);
  const std::uint32_t poolEnd = core.mem_r32(kPacketPoolEnd);
  // An in-between links into the pool the guest's own field drew into, which no pass reads.
  return FieldWindows{
      .scratch = kFacts.scratch,
      .tableBase = tableBase,
      .arenaEnd = poolEnd,
      .inBetweenArena = {.address = poolEnd - kPacketPoolBytes, .bytes = kPacketPoolBytes},
      .owned = {
          GuestSpan{spyro::guest_terrain::kScratchpad, spyro::guest_terrain::kScratchpadBytes},
          GuestSpan{kFacts.scratch, kScratchBlockBytes},
          GuestSpan{tableBase, kOrderingTableBytes},
          GuestSpan{},
          GuestSpan{}}};
}

spyro::guest_camera::Matrices
TerrainImage::camera(Core &core, const spyro::guest_camera::Angles &angles) const {
  return CameraBuilder(core, kRenderGlobals.cameraSineTable)
      .build(angles);
}

void TerrainImage::drawPasses(spyro::guest_terrain::TerrainFrame &frame) const {
  Core &core = frame.core;
  listSectors(frame, kFacts, static_cast<std::int32_t>(frame.argument));
  // 800261A8: the projection rotation, the packet cursor and the ordering table.
  core.gte().loadRotation(frame.rotation());
  frame.primitive = frame.primitiveBase();
  frame.orderingTable = frame.memory.mem_r32(kRenderGlobals.orderingTable);
  if (frame.memory.mem_r32(spyro1::guest::kSkipLowPolyWorld) == 0) {
    drawFarSectors(frame, kFacts);
  }
  drawDetailSectors(frame, kFacts);
  splitCoarsePolygons(frame, kFacts);
  splitFinePolygons(frame, kFacts);
  spyro::guest_terrain::resplitOversizedPrimitives(frame);
}

namespace {

void drawTerrain(Core *core) {
  spyro::guest_terrain::drawField(*core, kTerrainImage);
}

} // namespace

void registerOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 kSharedFacts.entry,
                                 kSharedFacts.overrideName,
                                 drawTerrain,
                                 std::make_unique<spyro::guest_terrain::TerrainStateProducer>(core, kTerrainImage));
}

} // namespace spyro1::terrain
