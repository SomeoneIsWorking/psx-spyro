#include "render/hud/ui_anchor.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>

namespace spyro::ui_anchor {
namespace {

const char *channel() {
  return "uihud";
}

} // namespace

Frame widescreenFrame(const Core &core, std::uint32_t authoredWidth) {
  const psx::present::GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  if (!plan.widescreen()) {
    return Frame{.authored = static_cast<std::int32_t>(authoredWidth),
                 .drawn = static_cast<std::int32_t>(authoredWidth)};
  }
  return Frame{.authored = plan.nativeExtent.width, .drawn = plan.presentationExtent.width};
}

std::int32_t correctionAndReport(const Element &element, Anchor anchor, const Frame &frame) {
  const auto shift = correction(anchor, frame);
  if (!shift) {
    lucent::warn(channel(),
                 "REFUSED element={} index={} anchor={} reason={} frame={}->{}",
                 element.name,
                 element.index,
                 name(anchor),
                 refusalName(checkFrame(frame)),
                 frame.authored,
                 frame.drawn);
    return 0;
  }
  lucent::debug(channel(),
                "element={} index={} anchor={} correction={} frame={}->{}",
                element.name,
                element.index,
                name(anchor),
                *shift,
                frame.authored,
                frame.drawn);
  return *shift;
}

} // namespace spyro::ui_anchor
