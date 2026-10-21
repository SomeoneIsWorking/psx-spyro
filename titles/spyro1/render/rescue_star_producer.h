// rescue_star_producer.h — 0x80058864 as a state producer: the override draws the star and saves
// the camera and the star block; the render draws it again over host memory at t between two saves,
// the centre, rotation, scale and grey moved under the in-between camera.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/render/rescue_star.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::rescue_star {

struct StarState {
  spyro::guest_terrain::CameraState camera;
  std::array<std::uint8_t, kBytes> block{};
};
static_assert(std::is_trivially_copyable_v<StarState>);

class RescueStarProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80058864u;

  explicit RescueStarProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerRescueStarOverrides(Core &core);

} // namespace spyro1::rescue_star
