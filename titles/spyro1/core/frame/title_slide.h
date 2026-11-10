// title_slide.h — the title screen's logo and border slides as a motion source. TitlescreenDraw
// 0x8007CEE4 places them at g_TitlescreenAnimIn/Out[m_SubTick], one byte per tick, so an in-between
// writes the entry the later tick reads `t` of the way from the one the earlier tick read.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class TitleSlide final : public psx::present::MotionSource {
public:
  void sample(Core &core) override;
  void write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  // The table entry this tick's draw reads, its value and the aligned word holding it.
  struct Entry {
    std::uint32_t subState = 0;
    std::uint32_t table = 0;
    std::uint32_t index = 0;
    std::uint8_t value = 0;
    std::uint32_t word = 0;
  };
  [[nodiscard]] static std::optional<Entry> entryOf(Core &core);

  std::optional<Entry> from_;
  std::optional<Entry> to_;
};

} // namespace spyro1
