// A logo is the game's OWN wordmark, rebuilt from the draws its title screen made from textures the
// game uploaded, at the moment the title screen's own gate says the logo is up; nothing here is
// authored art.
#pragma once

#include "drawn_logo.h"

#include <cstdint>
#include <span>

namespace spyro {

// The title screen's own gate: the game is in its title-screen state and, per title, the overlay
// block's tick has reached the logo (Spyro 1: `m_CurrentTick >= 1170`) or a title-screen sub-state
// word holds a value.
struct TitleLogoGate {
  std::uint32_t gameStateGlobal = 0;  // the guest word holding the game state
  std::uint32_t titleScreenState = 0; // its value while the title screen runs
  std::uint32_t pointerGlobal = 0;    // the guest word that HOLDS the block pointer (0: no block)
  std::uint32_t tickOffset = 0;       // the tick's offset inside that block
  std::uint32_t tickValue = 0;        // the tick at which the logo is up
  std::uint32_t subStateGlobal = 0;   // a title-screen sub-state word (0: none)
  std::uint32_t subStateMask = 0xFFFFFFFFu; // the bits of that word the sub-state uses
  std::uint32_t subStateValue = 0;          // their value while the logo stands still
};

struct TitleLogoFacts {
  TitleLogoGate gate{};
  // The textures the title screen draws its wordmark from; none means the title has no logo.
  std::span<const psx::present::LogoTexture> textures{};
};

} // namespace spyro
