// SPDX-License-Identifier: GPL-3.0-or-later
// A logo is the game's OWN sprite, drawn from a texture and CLUT the game uploaded, at the moment
// the title screen's own gate says the logo is up; nothing here is authored art.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace spyro {

// The title screen's own gate: the guest keeps a pointer global, the overlay's state block hangs
// off it, and the block's tick is what retail itself tests (Spyro 1: `m_CurrentTick >= 1170`).
struct TitleLogoGate {
  std::uint32_t pointerGlobal = 0; // the guest word that HOLDS the pointer (not the pointer itself)
  std::uint32_t tickOffset = 0;    // the tick's offset inside that block
  std::uint32_t tickValue = 0;     // the tick at which the logo is up
};

// A wordmark is the logo sprite and at most a banner beside it.
inline constexpr std::size_t kTitleLogoMaximumParts = 8;

// The wordmark's own sprite records, in composition order: each address is a record in the game's
// own sprite table, read from the guest at the moment the gate is open.
struct TitleLogoPart {
  std::uint32_t record = 0; // guest address of one 8-byte sprite record
};

struct TitleLogoFacts {
  // PanelLogo refuses invalid facts rather than snapshotting a rectangle that is not the wordmark.
  bool valid = false;
  TitleLogoGate gate{};
  std::span<const TitleLogoPart> parts{};
};

} // namespace spyro