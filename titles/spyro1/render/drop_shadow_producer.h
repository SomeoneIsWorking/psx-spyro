// drop_shadow_producer.h — DrawSpyroDropShadow 0x80059A48 as a state producer: the override
// draws the shadow and saves the camera, Spyro's record and the shadow block it was drawn from; the
// render draws it again over host memory at t between two saves, the ground point, heading and
// camera moved.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/drop_shadow.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::shadow {

struct ShadowState {
  spyro::guest_terrain::CameraState camera;
  std::array<std::uint8_t, actors::spyro_record::kBytes> spyro{};
  std::array<std::uint8_t, shadow_block::kBytes> shadow{};
};
static_assert(std::is_trivially_copyable_v<ShadowState>);

class DropShadowProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80059A48u;

  explicit DropShadowProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerDropShadowOverrides(Core &core);

} // namespace spyro1::shadow
