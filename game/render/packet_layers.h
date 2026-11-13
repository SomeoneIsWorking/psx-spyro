// packet_layers.h — the screen-space packets of a Spyro frame its linkers name: the UI crop
// widescreen (psxport plan 0203) shows at its 4:3 size and place over the cropped world, and the 2D
// font's glyphs (Spyro 2 and 3) psxport draws from outline fonts (plan 0207).
#pragma once

#include "crop_ui_layers.h"
#include "emit_memory.h"
#include "outline_text.h"

#include <cstdint>
#include <optional>
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

class PacketLayers final : public psx::present::CropUiLayers, public psx::present::OutlineGlyphs {
public:
  // A UI packet was linked.
  void linked(Core &core, std::uint32_t packet);
  // A UI chain, `first` to `last`, was linked.
  void linkedChain(Core &core,
                   const psx::present::EmitMemory &memory,
                   std::uint32_t first,
                   std::uint32_t last);
  // A text layout linked its glyphs, `first` to `last`.
  void
  linkedGlyphs(const psx::present::EmitMemory &memory, std::uint32_t first, std::uint32_t last);
  // The flatten: the packets linked since the last are the frame DrawOTag draws next, all of them
  // UI when `wholeFrame`.
  void flattened(bool wholeFrame);
  // The next flatten is a second pass's, chained after the first: its packets join the same
  // frame's.
  void secondPass();
  // Whether the packet at `packet` of the frame last flattened is UI.
  [[nodiscard]] bool uiPacket(std::uint32_t packet) const override;
  // The 2D font, when the packet at `packet` of the frame last flattened draws one of its cells.
  [[nodiscard]] std::optional<std::uint8_t> glyphFont(std::uint32_t packet) const override;

private:
  // Main-RAM offsets of a layer's packets: those linked since the flatten, and the frame's,
  // ascending.
  struct Layer {
    std::vector<std::uint32_t> linking;
    std::vector<std::uint32_t> drawn;

    void link(const psx::present::EmitMemory &memory, std::uint32_t first, std::uint32_t last);
    void flatten(bool joinsFrame);
    [[nodiscard]] bool holds(std::uint32_t packet) const;
  };

  Layer ui_;
  Layer glyphs_;
  bool wholeFrame_ = false;
  bool secondPass_ = false;
};

} // namespace spyro
