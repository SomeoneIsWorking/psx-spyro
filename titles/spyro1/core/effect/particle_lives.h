// particle_lives.h — which life of each g_Particles slot a sample holds: the allocator 0x80053570
// begins one.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstddef>
#include <cstdint>

class Core;

namespace spyro1 {

class ParticleLives final : public psx::present::MotionIncarnation {
public:
  static constexpr std::size_t kSlots = 256;

  // The allocator stamped the slot at `slot`.
  void allocated(Core &core, std::uint32_t slot);
  [[nodiscard]] std::uint32_t of(Core &core, std::uint32_t record) const override;

private:
  [[nodiscard]] static std::size_t indexOf(Core &core, std::uint32_t record);

  std::array<std::uint32_t, kSlots> lives_{};
};

} // namespace spyro1
