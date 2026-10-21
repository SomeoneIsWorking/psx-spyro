// moby_shadow_producer.h — the moby shadows 0x80059F8C as a state producer, one object per moby
// record: the override draws each shadow under its own emission scope and saves the camera, the
// record and its radius; the render draws it again over host memory at t between two saves, the
// moby's ground point and the camera moved.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/render/actor_between.h"
#include "state_producer.h"

#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::shadows {

struct MobyShadowState {
  spyro::guest_terrain::CameraState camera;
  std::uint32_t record = 0;
  std::uint32_t radius = 0;
  spyro::moby_record::Bytes bytes{};
};
static_assert(std::is_trivially_copyable_v<MobyShadowState>);

class MobyShadowProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80059F8Cu;

  explicit MobyShadowProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerMobyShadowOverrides(Core &core);

} // namespace spyro1::shadows
