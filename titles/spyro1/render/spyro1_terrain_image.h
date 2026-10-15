// spyro1_terrain_image.h — SCUS_942.28's terrain drawer (0x800258F0) as the engine family's
// `TerrainImage`: its sector loop, far, detail and split passes, the shared re-split pass, and
// the camera builder 0x80033C50 an in-between rebuilds its camera with.
#pragma once

#include "guest_camera_builder.h"
#include "guest_terrain_drawer.h"
#include "spyro1_terrain_facts.h"

namespace spyro1::terrain {

// 0x80033C50: (Ry * Rz) * Rx through libgte's MulMatrix, and row 1 scaled by a C division, which
// rounds toward zero.
class CameraBuilder final : public spyro::guest_camera::Builder {
public:
  using Builder::Builder;

private:
  [[nodiscard]] spyro::guest_camera::Rotation
  compose(const spyro::guest_camera::Rotation &x,
          const spyro::guest_camera::Rotation &y,
          const spyro::guest_camera::Rotation &z) const override {
    return multiply(multiply(y, z), x);
  }
  [[nodiscard]] std::int32_t scaleVertical(std::int32_t element) const override {
    return (element * spyro::guest_camera::kVerticalScale) / 512;
  }
};

class TerrainImage final : public spyro::guest_terrain::TerrainImage {
public:
  constexpr TerrainImage() = default;

  [[nodiscard]] const spyro::guest_terrain::Facts &facts() const override {
    return kSharedFacts;
  }
  [[nodiscard]] const spyro::guest_render_globals::Globals &globals() const override {
    return kRenderGlobals;
  }
  [[nodiscard]] spyro::guest_terrain::FieldWindows windows(Core &core) const override;
  [[nodiscard]] spyro::guest_camera::Matrices
  camera(Core &core, const spyro::guest_camera::Angles &angles) const override;
  void drawPasses(spyro::guest_terrain::TerrainFrame &frame) const override;
};

inline constexpr TerrainImage kTerrainImage{};

// The drawer's override and its state producer. Both callers pass $a0 = -1, its one object.
void registerOverrides(Core &core);

} // namespace spyro1::terrain
