#include "render/hud/ui_anchor.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>

namespace spyro::ui_anchor {

std::int32_t correction(const Core &core, const Element &element, Anchor anchor) {
  const psx::present::GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  const std::int32_t margin = plan.widescreen() ? plan.presentationHorizontalMargin : 0;
  const std::int32_t shift = anchor == Anchor::LeftEdge ? -margin : anchor == Anchor::RightEdge ? margin : 0;
  lucent::debug("uihud", "element={} index={} anchor={} correction={}", element.name, element.index, name(anchor), shift);
  return shift;
}

void shiftArgument(Core &core, std::uint32_t reg, const Element &element, Anchor anchor) {
  core.r[reg] = static_cast<std::uint32_t>(static_cast<std::int32_t>(core.r[reg]) + correction(core, element, anchor));
}

} // namespace spyro::ui_anchor
