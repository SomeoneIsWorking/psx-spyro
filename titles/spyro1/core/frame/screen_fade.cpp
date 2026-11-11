#include "spyro1/core/frame/screen_fade.h"

#include "blend.h"
#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

// 0x8001ED5C: g_Fade * 8 in play; 0x8007DCFC: g_Fade << 4 on the title screen.
constexpr unsigned kPlayingShift = 3;
constexpr unsigned kTitleShift = 4;
constexpr unsigned kGamestateShift = 16;
constexpr unsigned kModeShift = 8;

unsigned levelShift(Core &core) {
  return core.mem_r32(guest::kGamestate) == guest::kGamestateTitleScreen ? kTitleShift
                                                                         : kPlayingShift;
}

} // namespace

std::optional<ScreenFade::Read> ScreenFade::readOf(Core &core) {
  const std::uint32_t state = core.mem_r32(guest::kGamestate);
  if (state == guest::kGamestatePlaying) {
    return Read{.phase = state << kGamestateShift, .fade = core.mem_r32(guest::kFade)};
  }
  if (state != guest::kGamestateTitleScreen) {
    return std::nullopt;
  }
  return Read{.phase = state << kGamestateShift |
                       core.mem_r32(guest::kTitlescreenMode) << kModeShift |
                       core.mem_r32(guest::kTitlescreenSubState),
              .fade = core.mem_r32(guest::kFade)};
}

void ScreenFade::sample(Core &core) {
  from_ = to_;
  to_ = readOf(core);
}

bool ScreenFade::paired() const {
  return from_ && to_ && from_->phase == to_->phase;
}

psx::present::MotionCount ScreenFade::write(psx::present::GuestRedraw &, float) const {
  if (!to_ || to_->fade == 0) {
    return {};
  }
  return {.drawn = 1, .unpaired = paired() ? 0u : 1u};
}

std::optional<std::uint32_t> ScreenFade::level(Core &core, std::optional<float> t) const {
  const unsigned shift = levelShift(core);
  std::uint32_t shown = core.mem_r32(guest::kFade) << shift;
  if (t && from_ && to_ && from_->phase == to_->phase) {
    shown = static_cast<std::uint32_t>(
        psx::present::lerpInt(static_cast<std::int32_t>(from_->fade << shift),
                              static_cast<std::int32_t>(to_->fade << shift),
                              *t));
  }
  return shown != 0 ? std::optional(shown) : std::nullopt;
}

} // namespace spyro1
