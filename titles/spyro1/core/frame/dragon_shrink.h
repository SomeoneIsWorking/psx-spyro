// dragon_shrink.h — the freed dragon's shrink, GS_Dragon m_State 5 (0x8001CFDC, 0x8001D31C to
// 0x8001D438): after the moby draw's setup it scales the moby scale vector by D_8006F3C0[ticks]
// and, past tick 8, the second one by Cos((ticks - 8) * 42). The tick is a whole index, so an
// in-between scales by the table entry and the angle `t` of the way between the two ticks'.
#pragma once

#include "guest_redraw.h"
#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class DragonShrink final : public psx::present::MotionSource {
public:
  // The scale block's first instruction, after func_8001F158 returns, and where its branches join.
  static constexpr std::uint32_t kScaleBlock = 0x8001D31Cu;
  static constexpr std::uint32_t kScaleBlockEnd = 0x8001D438u;

  [[nodiscard]] const char *name() const override {
    return "dragon shrink";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The scale block's writes, `t` of the way between the ticks while both are m_State 5's.
  void scale(Core &core, psx::present::GuestRedraw &redraw, std::optional<float> t) const;

private:
  // m_CutsceneTicks while m_State is 5.
  [[nodiscard]] static std::optional<std::uint32_t> ticksOf(Core &core);

  std::optional<std::uint32_t> from_;
  std::optional<std::uint32_t> to_;
};

} // namespace spyro1
