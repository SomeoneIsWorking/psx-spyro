// spyro1_glow_producers.h — the two glow routines of DrawWorldParticleEmitters 0x80058BA8 as state
// producers, one object per emitter or sparkle: the override draws each under its own emission
// scope and saves what it was drawn from; the render draws it again over host memory at t between
// two saves, its position and the camera moved.
#pragma once

#include "spyro1_glows.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::glows {

struct EmitterState {
  spyro::guest_terrain::CameraState camera;
  std::uint32_t emitter = 0;
  std::array<std::uint8_t, kEmitterBytes> bytes{};
  // The three words its position pointer names.
  std::array<std::uint32_t, 3> position{};
};
static_assert(std::is_trivially_copyable_v<EmitterState>);

// A sparkle as the field left it: aged by that field's delta, so the render draws it with none.
struct SparkleState {
  spyro::guest_terrain::CameraState camera;
  std::uint32_t record = 0;
  std::array<std::uint8_t, kSparkleBytes> bytes{};
};
static_assert(std::is_trivially_copyable_v<SparkleState>);

class GlowFanProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x800580F4u;

  explicit GlowFanProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

class SparkleProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x800584C4u;

  explicit SparkleProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// A sparkle at `t`: position and angle moved, everything else `to`'s.
[[nodiscard]] std::array<std::uint8_t, kSparkleBytes>
sparkleBetween(const std::array<std::uint8_t, kSparkleBytes> &from,
               const std::array<std::uint8_t, kSparkleBytes> &to,
               float t);

// Both overrides and their renders.
void registerGlowOverrides(Core &core);

} // namespace spyro1::glows
