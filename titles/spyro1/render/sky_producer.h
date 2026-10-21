// sky_producer.h — the cyclorama drawer 0x8004EBA8 as a state producer: the override draws
// the sky and saves the rotations and camera it was drawn under; the render draws it again over
// host memory with the camera at t between two saves.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/render/sky.h"
#include "state_producer.h"

#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::sky {

struct SkyState {
  spyro::guest_terrain::CameraState camera;
  Rotations rotations;
  std::int32_t group = 0;
  // Non-zero when $a1 and $a2 were the camera's own view and projection.
  std::uint32_t cameraRotations = 0;
};
static_assert(std::is_trivially_copyable_v<SkyState>);

class SkyProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8004EBA8u;

  explicit SkyProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerSkyOverrides(Core &core);

} // namespace spyro1::sky
