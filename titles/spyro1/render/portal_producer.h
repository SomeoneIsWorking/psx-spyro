// portal_producer.h — the portal drawer 0x80050BD0 as a state producer, one object per portal: the
// override draws each portal's fill and near sky under its own emission scope and saves the camera,
// the sky's spin and the level clock; the render draws them again over host memory at t between two
// saves.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/render/portals.h"
#include "state_producer.h"

#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::portals {

struct PortalState {
  spyro::guest_terrain::CameraState camera;
  SkySpin spin;
  std::uint32_t index = 0;
  std::uint32_t ticks = 0;
};
static_assert(std::is_trivially_copyable_v<PortalState>);

class PortalProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80050BD0u;

  explicit PortalProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerPortalOverrides(Core &core);

} // namespace spyro1::portals
