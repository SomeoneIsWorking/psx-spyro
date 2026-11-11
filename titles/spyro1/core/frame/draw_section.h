// draw_section.h — GamestateDraw 0x8001ED5C as a replay for GS_Playing and the title screen: the
// buffer flip it opens with, CameraUpdateMatrices, GS_Playing's drawers in order or the title's
// own draw to its device tail, and the bin flatten whose chain DrawOTag submits.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class DrawSection final : public psx::present::ReplaySection {
public:
  // Whether this frame's GamestateDraw is one the section replays.
  [[nodiscard]] bool replays(Core &core) const;

  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core,
                                                  psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // GS_Playing's drawers, from QueueMobys to the HUD.
  [[nodiscard]] static bool drawPlaying(Core &core, psx::present::GuestRedraw &redraw);
  // TitlescreenDraw 0x8007CEE4 itself up to its fade, then the fade at the replay's level.
  [[nodiscard]] static bool drawTitle(Core &core, psx::present::GuestRedraw &redraw);
};

} // namespace spyro1
