// shadow_block.h — g_SpyroShadow (D_8007AA10): the ground-shadow state the effect update
// writes and DrawSpyroDropShadow 0x80059A48 reads.
#pragma once

#include <cstdint>

namespace spyro1::shadow_block {

inline constexpr std::uint32_t kBlock = 0x8007AA10u;
inline constexpr std::uint32_t kBytes = 0x28u;
inline constexpr std::uint32_t kHeights = kBlock + 0x00u;   // eight signed bytes, one per slot
inline constexpr std::uint32_t kSlotFlags = kBlock + 0x08u; // eight bytes; any set shrinks the ring
inline constexpr std::uint32_t kAnchor = kBlock + 0x10u;    // the ground point
inline constexpr std::uint32_t kReference = kBlock + 0x18u; // the reference height
inline constexpr std::uint32_t kBias = kBlock + 0x1Cu;      // the depth bias
inline constexpr std::uint32_t kRingIndex = kBlock + 0x20u; // the slot the update writes next
inline constexpr std::uint32_t kSuppress = kBlock + 0x24u;  // positive hides the shadow

} // namespace spyro1::shadow_block
