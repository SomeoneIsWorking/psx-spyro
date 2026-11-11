#include "spyro1/core/frame/title_slide.h"

#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {

std::optional<TitleSlide::Entry> TitleSlide::entryOf(Core &core) const {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateTitleScreen) {
    return std::nullopt;
  }
  const std::uint32_t mode = core.mem_r32(guest::kTitlescreenMode);
  const std::uint32_t subState = core.mem_r32(guest::kTitlescreenSubState);
  const std::uint32_t subTick = core.mem_r32(guest::kTitlescreenSubTick);
  Entry entry{.phase = mode << 8u | subState};
  const bool init = mode == guest::kTitleModeInit;
  if (mode == guest::kTitleModeLoading &&
      core.mem_r32(guest::kTitlescreenStage) >= guest::kTitleLoadingLeave) {
    // The save picker's borders slide out from m_State 5 on.
    entry.table = guest::kTitlescreenAnimOut;
    entry.index = subTick;
  } else if (init && subState == guest::kTitleLogoIn) {
    const std::uint32_t layout = core.mem_r32(guest::kCutsceneLayout);
    if (static_cast<std::int32_t>(core.mem_r32(layout + guest::kCutsceneLayoutCurrentTick)) <
        guest::kTitleLogoInTick) {
      return std::nullopt;
    }
    entry.table = guest::kTitlescreenAnimIn;
    entry.index = subTick;
  } else if (init && subState == guest::kTitleLogoOut) {
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
  return entry;
}

} // namespace spyro1
