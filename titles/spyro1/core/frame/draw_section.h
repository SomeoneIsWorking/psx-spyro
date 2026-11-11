// draw_section.h — GamestateDraw 0x8001ED5C as a replay for GS_Playing, GS_Dragon, GS_Cutscene
// and the title screen: the buffer flip it opens with, CameraUpdateMatrices,
// GS_Playing's drawers in order or the state's own draw to its device tail, and the bin flatten
// whose chain DrawOTag submits.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class DrawSection final : public psx::present::ReplaySection {
public:
  // Whether this frame's GamestateDraw is one the section replays.
  [[nodiscard]] bool replays(Core &core) const;
  // The draw's stage: the gamestate, and GS_Dragon's m_State, whose steps each re-place the camera.
  [[nodiscard]] static std::uint32_t stage(Core &core);

  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core,
                                                  psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;

private:
  // GS_Playing's drawers, from QueueMobys to the HUD.
  [[nodiscard]] static bool drawPlaying(Core &core, psx::present::GuestRedraw &redraw);
  // The current gamestate's draw.
  [[nodiscard]] static bool drawState(Core &core, psx::present::GuestRedraw &redraw);
  // A state draw `entry` with no override of its own, from its start to `end`.
  [[nodiscard]] static bool
  drawTo(psx::present::GuestRedraw &redraw, std::uint32_t entry, std::uint32_t end);
  // A state draw up to `fade`, where it loads g_Fade, then the fade at the replay's level.
  [[nodiscard]] static bool drawToFade(Core &core,
                                       psx::present::GuestRedraw &redraw,
                                       std::uint32_t entry,
                                       std::uint32_t fade);
};

} // namespace spyro1
