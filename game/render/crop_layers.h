// crop_layers.h — the packets of a Spyro frame that crop widescreen (psxport plan 0203) shows as
// UI, at their 4:3 size and place over the cropped world: the screen-space packets each title's
// linkers name as they link them.
#pragma once

#include "crop_ui_layers.h"
#include "emit_memory.h"

#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace spyro {

// The game states whose frames are laid out for 4:3 as a whole (a pause screen), which crop shows
// all as UI.
struct WholeFrameUi {
  std::uint32_t gameState = 0;
  std::span<const std::uint32_t> states;

  [[nodiscard]] bool covers(Core &core) const;
};

class CropLayers final : public psx::present::CropUiLayers {
public:
  // A UI packet was linked.
  void linked(Core &core, std::uint32_t packet);
  // A UI chain, `first` to `last`, was linked.
  void linkedChain(Core &core,
                   const psx::present::EmitMemory &memory,
                   std::uint32_t first,
                   std::uint32_t last);
  // The flatten: the packets linked since the last are the frame DrawOTag draws next, all of them
  // UI when `wholeFrame`.
  void flattened(bool wholeFrame);
  // The next flatten is a second pass's, chained after the first: its packets join the same
  // frame's.
  void secondPass();
  // Whether the packet at `packet` of the frame last flattened is UI.
  [[nodiscard]] bool uiPacket(std::uint32_t packet) const override;

private:
  std::vector<std::uint32_t> linking_; // main-RAM offsets
  std::vector<std::uint32_t> drawn_;   // the same, ascending
  bool wholeFrame_ = false;
  bool secondPass_ = false;
};

} // namespace spyro
