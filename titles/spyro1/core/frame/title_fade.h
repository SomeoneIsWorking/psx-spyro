// title_fade.h — the title screen's fade as a motion source. TitlescreenDraw draws
// DrawFade(2, l, l, l) with l = g_Fade << 4 while g_Fade is set (0x8007DCE8..0x8007DD04);
// TSM_Init's sub-state 5 and the save picker's m_State 5 raise g_Fade one a tick, so an in-between
// draws the level `t` of the way between the two ticks' levels, which no whole g_Fade can name.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class TitleFade final : public psx::present::MotionSource {
public:
  [[nodiscard]] const char *name() const override {
    return "title fade";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The level TitlescreenDraw's fade draws now, or at `t` while both ticks are in one title phase;
  // none at 0, where the draw skips it.
  [[nodiscard]] std::optional<std::uint32_t> level(Core &core, std::optional<float> t) const;

private:
  // m_Mode and m_SubState, and g_Fade, as one tick's draw reads them.
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
