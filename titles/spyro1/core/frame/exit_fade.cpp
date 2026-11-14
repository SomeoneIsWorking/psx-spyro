#include "spyro1/core/frame/exit_fade.h"

#include "blend.h"
#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

// g_nMenuDrawFrames 1: the pass after the backdrop's, which fogs the sky.
constexpr std::uint32_t kFogPass = 1u;
constexpr std::int32_t kFadeTicks = 0x20;
constexpr unsigned kProgressShift = 7;

std::int32_t progressOf(std::uint32_t ticks) {
  return (kFadeTicks - static_cast<std::int32_t>(ticks)) * (1 << kProgressShift);
}

} // namespace

std::optional<std::uint32_t> ExitFade::ticksOf(Core &core) {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateExitLevel ||
      core.mem_r32(guest::kMenuDraws) != kFogPass) {
    return std::nullopt;
  }
  return core.mem_r32(guest::kMenuTicks);
}

void ExitFade::sample(Core &core) {
  from_ = to_;
  to_ = ticksOf(core);
}

psx::present::MotionCount ExitFade::write(psx::present::GuestRedraw &, float) const {
  if (!to_) {
    return {};
  }
  return {.drawn = 1, .unpaired = from_ ? 0u : 1u};
}

std::optional<std::uint32_t> ExitFade::progress(std::optional<float> t) const {
  if (!t || !from_ || !to_) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(
      psx::present::lerpInt(progressOf(*from_), progressOf(*to_), *t));
}

} // namespace spyro1
