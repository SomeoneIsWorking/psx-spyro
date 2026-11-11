// screen_fade.h — g_Fade as a motion source for the two draws that read it. GamestateDraw's stage 0
// draws DrawFade(2, l, l, l) with l = g_Fade * 8 (0x8001ED5C) and TitlescreenDraw with l = g_Fade
// << 4 (0x8007DCE8..0x8007DD04). The level-entry fade falls by g_DeltaTime a tick, the demo
// fade-out and the title fade-outs rise one step a tick, so an in-between draws the level `t` of
// the way between the two ticks' levels, which no whole g_Fade can name.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class ScreenFade final : public psx::present::MotionSource {
public:
  [[nodiscard]] const char *name() const override {
    return "screen fade";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The level the current gamestate's fade draws now, or at `t` while both ticks are in one phase;
  // none at 0, where the draw skips it.
  [[nodiscard]] std::optional<std::uint32_t> level(Core &core, std::optional<float> t) const;

private:
  // The gamestate (with m_Mode and m_SubState on the title screen) and g_Fade, as one tick's draw
  // reads them.
  struct Read {
    std::uint32_t phase = 0;
    std::uint32_t fade = 0;
  };
  [[nodiscard]] static std::optional<Read> readOf(Core &core);
  [[nodiscard]] bool paired() const;

  std::optional<Read> from_;
  std::optional<Read> to_;
};

} // namespace spyro1
