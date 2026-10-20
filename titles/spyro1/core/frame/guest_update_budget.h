#pragma once

#include "emulated_time.h"

#include <cstdint>

namespace spyro1 {

// The bound one guest update has: four seconds, as guest cycles and as display fields.
inline constexpr std::uint64_t kGuestUpdateCycles = std::uint64_t{psx::frame::kNominalPsxCpuHz} * 4u;
inline constexpr std::uint32_t kGuestUpdateFields = 240u;

} // namespace spyro1
