#include "spyro1_terrain_image.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_passes.h"
#include "native_dispatch.h"
#include "spyro1_terrain_passes.h"
#include "spyro_session.h"
#include "terrain_state_producer.h"

#include <memory>

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
using spyro::guest_memory::GuestRange;
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
      .inBetweenArena = {.base = poolEnd - kPacketPoolBytes, .bytes = kPacketPoolBytes},
      .owned = {
          GuestRange{spyro::guest_terrain::kScratchpad, spyro::guest_terrain::kScratchpadBytes},
          GuestRange{kFacts.scratch, kScratchBlockBytes},
          GuestRange{tableBase, kOrderingTableBytes},
          GuestRange{},
          GuestRange{}}};
}

spyro::guest_camera::Matrices
TerrainImage::camera(Core &core, const spyro::guest_camera::Angles &angles) const {
  return CameraBuilder(
             core,
             {.sine = kRenderGlobals.cameraSineTable, .cosine = kRenderGlobals.cameraCosineTable})
      .build(angles);
}

void TerrainImage::drawPasses(spyro::guest_terrain::TerrainFrame &frame) const {
  Core &core = frame.core;
  listSectors(frame, kFacts, static_cast<std::int32_t>(frame.argument));
  // 800261A8: the projection rotation, the packet cursor and the ordering table.
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    core.gte().writeControl(gte::kRotation + i, frame.rotationWord(i));
  }
  frame.primitive = frame.primitiveBase();
  frame.orderingTable = frame.memory.r32(kRenderGlobals.orderingTable);
  if (frame.memory.r32(kFacts.farPassSkip) == 0) {
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
  spyro::session(core).stateProducers.install(
      kSharedFacts.entry,
      std::make_unique<spyro::guest_terrain::TerrainStateProducer>(core, kTerrainImage));
  psx::cpu::installNativeOverride(core,
                                  kSharedFacts.entry,
                                  kSharedFacts.overrideName,
                                  drawTerrain,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::terrain
