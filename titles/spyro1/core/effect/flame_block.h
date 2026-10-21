// flame_block.h — g_SpyroFlame (g_anSpyroHornStrikeAnchorPos 0x800786C8): the flame burst the
// effect update publishes, Spyro's draw orients and DrawSpyroHornStrikeTrails 0x80058D64 draws.
#pragma once

#include <cstdint>

namespace spyro1::flame_block {

inline constexpr std::uint32_t kBlock = 0x800786C8u;
inline constexpr std::uint32_t kBytes = 0xCCu;
inline constexpr std::uint32_t kParts = 8;
inline constexpr std::uint32_t kPosition = kBlock + 0x00u; // the anchor, x, y, z words
inline constexpr std::uint32_t kTexture = kBlock + 0x10u; // two ribbon UV words, +8 when superflame
inline constexpr std::uint32_t kLengths = kBlock + 0x20u; // eight signed bytes, one per part
inline constexpr std::uint32_t kLimits = kBlock + 0x28u;  // signed bytes, each part's last entry
inline constexpr std::uint32_t kActive = kBlock + 0x98u;  // m_IsFlameActive
inline constexpr std::uint32_t kState = kBlock + 0x99u;
inline constexpr std::uint32_t kSelect = kBlock + 0x9Au;  // asks Spyro's draw for the rotation
inline constexpr std::uint32_t kVariant = kBlock + 0x9Bu; // the rand() bit
inline constexpr std::uint32_t kSuper = kBlock + 0x9Cu;   // superflame word
inline constexpr std::uint32_t kFairyKissTimer = kBlock + 0xA0u; // m_FairyKissTimer
inline constexpr std::uint32_t kRotation = kBlock + 0xB8u;       // five RT words, view space
inline constexpr std::uint32_t kRotationBytes = 0x14u;

} // namespace spyro1::flame_block
