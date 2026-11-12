// game_state.h — SCUS_944.67's game state word and the states the render owners test.
#pragma once

#include <cstdint>

namespace spyro3 {

// The draw's switch operand (lw at 0x8001E6E0).
inline constexpr std::uint32_t kGameState = 0x8006E344u;
// The title screen's state.
inline constexpr std::uint32_t kTitleScreenGameState = 11u;

} // namespace spyro3
