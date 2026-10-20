#pragma once

#include "emulated_time.h"
#include "field_rate.h"

#include <cstdint>

namespace spyro1 {

// The bound one guest update has, as guest cycles and as NTSC display fields (rounded up).
inline constexpr std::uint32_t kGuestUpdateSeconds = 4u;
inline constexpr std::uint64_t kGuestUpdateCycles =
    std::uint64_t{psx::frame::kNominalPsxCpuHz} * kGuestUpdateSeconds;
inline constexpr std::uint32_t kGuestUpdateFields =
    (kGuestUpdateSeconds * psx::frame::DISPLAY_FIELD_RATE_NTSC.frequencyNumerator +
     psx::frame::DISPLAY_FIELD_RATE_NTSC.frequencyDenominator - 1u) /
    psx::frame::DISPLAY_FIELD_RATE_NTSC.frequencyDenominator;
static_assert(kGuestUpdateFields == 240u);

} // namespace spyro1
