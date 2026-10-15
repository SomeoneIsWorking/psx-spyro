// spyro1_particle_producer.h — RasterizeEmitList 0x800573C8 as a state producer, one object per
// particle record: the override draws each record under its own emission scope and saves what it
// was drawn from; the render draws it again over host memory at t between two saves, its position,
// angle and the camera moved.
#pragma once

#include "spyro1_particles.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::particles {

using RecordBytes = std::array<std::uint8_t, kRecordBytes>;

struct ParticleState {
  spyro::guest_terrain::CameraState camera;
  std::uint32_t record = 0;
  RecordBytes bytes{};
};
static_assert(std::is_trivially_copyable_v<ParticleState>);

class ParticleProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x800573C8u;

  explicit ParticleProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// A record at `t`: its points and angle moved when both saves hold the same kind, else `to`.
[[nodiscard]] RecordBytes particleBetween(const RecordBytes &from, const RecordBytes &to, float t);

// The override and its render.
void registerParticleOverrides(Core &core);

} // namespace spyro1::particles
