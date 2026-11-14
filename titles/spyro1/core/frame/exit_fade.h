// exit_fade.h — GS_ExitLevel's fog fade as a motion source. MenuTransition_Draw 0x8001C694's second
// pass stores g_nFadeProgress = (0x20 - g_nMenuTickFrames) << 7 (0x8001C8BC..0x8001C8C8) for
// EmitSkyMeshFogged, and SpyroStateDelay_Update 0x8002E084 raises the tick by one a tick, so an
// in-between fogs the sky `t` of the way between the two ticks' progress, which no whole tick
// names.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro1 {

class ExitFade final : public psx::present::MotionSource {
public:
  [[nodiscard]] const char *name() const override {
    return "exit fade";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The progress the fog pass draws at `t` while both ticks are in it; none otherwise, where the
  // guest's own store stands.
  [[nodiscard]] std::optional<std::uint32_t> progress(std::optional<float> t) const;

private:
  // g_nMenuTickFrames while GS_ExitLevel is in its fog pass.
  [[nodiscard]] static std::optional<std::uint32_t> ticksOf(Core &core);

  std::optional<std::uint32_t> from_;
  std::optional<std::uint32_t> to_;
};

} // namespace spyro1
