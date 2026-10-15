// engine_terrain_image.h — the terrain drawer SCUS_944.25 and SCUS_944.67 share (80023BCC on
// SCUS_944.25), as one `TerrainImage` bound to an image's facts.
#pragma once

#include "guest_render_globals.h"
#include "guest_terrain_drawer.h"
#include "guest_terrain_facts.h"

namespace spyro::guest_terrain {

class EngineTerrainImage final : public TerrainImage {
public:
  constexpr EngineTerrainImage(const Facts &facts, const guest_render_globals::Globals &globals)
      : facts_(facts), globals_(globals) {}

  [[nodiscard]] const Facts &facts() const override {
    return facts_;
  }
  [[nodiscard]] const guest_render_globals::Globals &globals() const override {
    return globals_;
  }
  // The scratch block below the frame's scratch block end, its flag window, the ordering table and
  // the fogged colours.
  [[nodiscard]] FieldWindows windows(Core &core) const override;
  // FUN_8001c2f8.
  [[nodiscard]] guest_camera::Matrices camera(Core &core,
                                              const guest_camera::Angles &angles) const override;
  // The sector-visibility call, then the seven passes (guest_terrain_passes.h).
  void drawPasses(TerrainFrame &frame) const override;

private:
  const Facts &facts_;
  const guest_render_globals::Globals &globals_;
};

} // namespace spyro::guest_terrain
