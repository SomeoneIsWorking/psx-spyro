// spyro1_paired_actor_producer.h — RasterizePairedActor 0x80023AC4 as a state producer: the
// override draws Spyro under his record's emission scope and saves the camera and record he was
// drawn from; the render draws him again over host memory at t between two saves, position and
// angles moved.
#pragma once

#include "spyro1_actor_facts.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::actors {

struct PairedActorState {
  spyro::guest_terrain::CameraState camera;
  std::array<std::uint8_t, spyro_record::kBytes> record{};
};
static_assert(std::is_trivially_copyable_v<PairedActorState>);

class PairedActorProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80023AC4u;

  explicit PairedActorProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// Spyro's record at `t`: position and the three parts' angle bytes moved, everything else `to`'s.
[[nodiscard]] std::array<std::uint8_t, spyro_record::kBytes>
spyroBetween(const std::array<std::uint8_t, spyro_record::kBytes> &from,
             const std::array<std::uint8_t, spyro_record::kBytes> &to,
             float t);

// The override and its render.
void registerPairedActorOverrides(Core &core);

} // namespace spyro1::actors
