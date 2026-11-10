// draw_section.h — GamestateDraw 0x8001ED5C's GS_Playing section as a replay: the buffer flip it
// opens with, CameraUpdateMatrices and its drawers in order, and the bin flatten whose chain
// DrawOTag submits.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class DrawSection final : public psx::present::ReplaySection {
public:
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core,
                                                  psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;
};

} // namespace spyro1
