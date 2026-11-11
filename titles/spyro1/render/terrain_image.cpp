#include "spyro1/render/terrain_image.h"

#include "core.h"
#include "gte_registers.h"
#include "native_dispatch.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_drawer.h"
#include "render/terrain/guest_terrain_passes.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/terrain_passes.h"

#include <optional>

namespace spyro1::terrain {
std::uint32_t TerrainImage::scratch(Core &) const {
  return kFacts.scratch;
}

spyro::guest_camera::Matrices
TerrainImage::camera(Core &core, const spyro::guest_camera::Angles &angles) const {
  return CameraBuilder(core, kRenderGlobals.cameraSineTable).build(angles);
}

spyro::guest_camera::Angles TerrainImage::cameraAngles(Core &core) {
  if (const std::optional<spyro::guest_camera::Angles> &replay = nativeState(core).replayCamera) {
    return *replay;
  }
  const std::uint32_t angles = kRenderGlobals.cameraAngles;
  const auto fine = [&core](std::uint32_t at) {
    return static_cast<std::int32_t>(core.mem_r16s(at)) << spyro::guest_camera::kFineBits;
  };
  return {.aroundX = fine(angles), .aroundY = fine(angles + 2), .aroundZ = fine(angles + 4)};
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
  spyro::guest_terrain::draw(*core, kTerrainImage);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kSharedFacts.entry, kSharedFacts.overrideName, drawTerrain);
  core.depthTest.mark(kSharedFacts.entry,
                      psx::present::DepthOrder::Polygon,
                      spyro::guest_render_globals::kDepthBinViewUnits);
}

} // namespace spyro1::terrain
