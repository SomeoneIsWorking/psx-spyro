// flame_producer.h — DrawSpyroHornStrikeTrails 0x80058D64 as a state producer: the override draws
// the flame and saves the camera and the flame block it was drawn from; the render draws it again
// over host memory at t between two saves, the anchor, the published rotation and the camera moved
// and each growing part one step on.
#pragma once

#include "render/terrain/terrain_rebuild.h"
#include "spyro1/core/effect/flame_block.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::flame {

struct FlameState {
  spyro::guest_terrain::CameraState camera;
  std::array<std::uint8_t, flame_block::kBytes> block{};
};
static_assert(std::is_trivially_copyable_v<FlameState>);

class FlameProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80058D64u;

  explicit FlameProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerFlameOverrides(Core &core);

} // namespace spyro1::flame
