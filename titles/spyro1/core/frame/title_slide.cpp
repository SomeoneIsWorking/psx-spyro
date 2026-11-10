#include "spyro1/core/frame/title_slide.h"

#include "blend.h"
#include "core.h"
#include "guest_redraw.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {

std::optional<TitleSlide::Entry> TitleSlide::entryOf(Core &core) {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateTitleScreen ||
      core.mem_r32(guest::kTitlescreenMode) != guest::kTitleModeInit) {
    return std::nullopt;
  }
  const std::uint32_t subState = core.mem_r32(guest::kTitlescreenSubState);
  const std::uint32_t subTick = core.mem_r32(guest::kTitlescreenSubTick);
  Entry entry{.subState = subState};
  if (subState == guest::kTitleLogoIn) {
    const std::uint32_t layout = core.mem_r32(guest::kCutsceneLayout);
    if (static_cast<std::int32_t>(core.mem_r32(layout + guest::kCutsceneLayoutCurrentTick)) <
        guest::kTitleLogoInTick) {
      return std::nullopt;
    }
    entry.table = guest::kTitlescreenAnimIn;
    entry.index = subTick;
  } else if (subState == guest::kTitleLogoOut) {
    entry.table = subTick < guest::kTitlescreenAnimSteps ? guest::kTitlescreenAnimOut
                                                         : guest::kTitlescreenAnimIn;
    entry.index =
        subTick < guest::kTitlescreenAnimSteps ? subTick : subTick - guest::kTitlescreenAnimSteps;
  } else {
    return std::nullopt;
  }
  if (entry.index >= guest::kTitlescreenAnimSteps) {
    return std::nullopt;
  }
  const std::uint32_t address = entry.table + entry.index;
  entry.value = core.mem_r8(address);
  entry.word = core.mem_r32(address & ~3u);
  return entry;
}

void TitleSlide::sample(Core &core) {
  from_ = to_;
  to_ = entryOf(core);
}

void TitleSlide::write(psx::present::GuestRedraw &redraw, float t) const {
  if (!from_ || !to_ || from_->subState != to_->subState || from_->table != to_->table ||
      to_->index != from_->index + 1) {
    return;
  }
  const std::uint32_t address = to_->table + to_->index;
  const unsigned shift = 8u * (address & 3u);
  const auto value = static_cast<std::uint32_t>(psx::present::lerpInt(from_->value, to_->value, t));
  redraw.writeWord(address & ~3u, (to_->word & ~(0xFFu << shift)) | (value << shift));
}

} // namespace spyro1
