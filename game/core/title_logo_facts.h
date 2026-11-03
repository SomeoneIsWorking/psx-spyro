// A logo is the game's OWN wordmark, rebuilt from the draws its title screen made from textures the
// game uploaded, at the moment the title screen's own gate says the logo is up; nothing here is
// authored art.
#pragma once

#include "drawn_logo.h"

#include <cstdint>
#include <span>

namespace spyro {

// The title screen's own gate: the game is in its title-screen state and its screen waits for input with
// the finished wordmark up (a sub-state word, and per title the screen's mode word).
struct TitleLogoGate {
  std::uint32_t gameStateGlobal = 0;  // the guest word holding the game state
  std::uint32_t titleScreenState = 0; // its value while the title screen runs
  std::uint32_t modeGlobal = 0;       // the title screen's mode word (0: none)
  std::uint32_t modeValue = 0;        // its value while the screen waits for input
  std::uint32_t subStateGlobal = 0;   // the title screen's sub-state word
  std::uint32_t subStateMask = 0xFFFFFFFFu; // the bits of that word the sub-state uses
  std::uint32_t subStateValue = 0;          // their value while the logo stands still
};

struct TitleLogoFacts {
  TitleLogoGate gate{};
  // The textures the title screen draws its wordmark from; none means the title has no logo.
  std::span<const psx::present::LogoTexture> textures{};
};

} // namespace spyro
