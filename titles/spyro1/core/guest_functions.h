// The Spyro 1 callee entries more than one native calls or installs, one name each.
#pragma once

#include <cstdint>

namespace spyro1::guest {

inline constexpr std::uint32_t kRand = 0x8006272Cu;
inline constexpr std::uint32_t kCopy3 = 0x80017700u;     // three-word vector copy
inline constexpr std::uint32_t kZero3 = 0x800176F0u;     // three-word vector clear
inline constexpr std::uint32_t kFill = 0x80016914u;      // word fill
inline constexpr std::uint32_t kCopyWords = 0x80016958u; // word copy
inline constexpr std::uint32_t kVadd = 0x80017758u;
inline constexpr std::uint32_t kVsub = 0x8001778Cu;
inline constexpr std::uint32_t kVsra = 0x800176C8u;      // arithmetic shift of a vector
inline constexpr std::uint32_t kVecLen = 0x800171FCu;
inline constexpr std::uint32_t kAngDiff8 = 0x80017908u;
inline constexpr std::uint32_t kAtan2 = 0x80016AB4u;
inline constexpr std::uint32_t kInitNewGameState = 0x8001277Cu; // InitNewGameState (open-spyro)

} // namespace spyro1::guest
