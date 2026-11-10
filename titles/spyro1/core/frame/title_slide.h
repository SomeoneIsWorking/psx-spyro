// title_slide.h — the title screen's logo and border slides as a table slide. TitlescreenDraw
// 0x8007CEE4 places them at g_TitlescreenAnimIn/Out[m_SubTick], one byte per tick.
#pragma once

#include "core/table_slide.h"

namespace spyro1 {

class TitleSlide final : public spyro::TableSlide {
private:
  [[nodiscard]] std::optional<Entry> entryOf(Core &core) const override;
};

} // namespace spyro1
