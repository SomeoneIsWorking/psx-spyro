// game_state.h — SCUS_944.25's game state word and the states the render owners test.
#pragma once

#include <cstdint>

namespace spyro2 {

// The draw's switch operand (lw in 0x800156FC).
inline constexpr std::uint32_t kGameState = 0x800681C8u;
// The title screen's state.
inline constexpr std::uint32_t kTitleScreenGameState = 11u;
// The title screen's own state word; 2 holds the finished logo while "press start" waits.
inline constexpr std::uint32_t kTitleScreenState = 0x80067E0Cu;
inline constexpr std::uint32_t kTitleScreenWaiting = 2u;

} // namespace spyro2
