#include "spyro1/core/frame/title_fade.h"

#include "blend.h"
#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

// 0x8007DCFC: the level is g_Fade << 4.
constexpr unsigned kLevelShift = 4;

} // namespace

std::optional<TitleFade::Read> TitleFade::readOf(Core &core) {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateTitleScreen) {
    return std::nullopt;
  }
  return Read{.phase = core.mem_r32(guest::kTitlescreenMode) << 8u |
                       core.mem_r32(guest::kTitlescreenSubState),
              .fade = core.mem_r32(guest::kFade)};
}

void TitleFade::sample(Core &core) {
  from_ = to_;
  to_ = readOf(core);
}

bool TitleFade::paired() const {
  return from_ && to_ && from_->phase == to_->phase;
}

psx::present::MotionCount TitleFade::write(psx::present::GuestRedraw &, float) const {
  if (!to_ || to_->fade == 0) {
    return {};
  }
  return {.drawn = 1, .unpaired = paired() ? 0u : 1u};
}

std::optional<std::uint32_t> TitleFade::level(Core &core, std::optional<float> t) const {
  std::uint32_t shown = core.mem_r32(guest::kFade) << kLevelShift;
  if (t && from_ && to_ && from_->phase == to_->phase) {
    shown = static_cast<std::uint32_t>(
        psx::present::lerpInt(static_cast<std::int32_t>(from_->fade << kLevelShift),
                              static_cast<std::int32_t>(to_->fade << kLevelShift),
                              *t));
  }
  return shown != 0 ? std::optional(shown) : std::nullopt;
}

} // namespace spyro1
