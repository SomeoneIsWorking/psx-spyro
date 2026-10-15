#include "render/field/guest_view_slope.h"

#include "game.h"

namespace spyro {

guest_moby_frustum::HorizontalSlope presentedSlope(const Core &core) {
  const GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  if (!plan.widescreen()) {
    return guest_moby_frustum::kRetailSlope;
  }
  return guest_moby_frustum::widenedSlope(plan.nativeProjectionExtent.width,
                                          plan.projectionExtent.width);
}

int presentedMargin(const Core &core) {
  const GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  return plan.widescreen() ? plan.projectionHorizontalMargin : 0;
}

guest_terrain::ScreenBounds presentedBounds(const Core &core, const guest_terrain::Facts &facts) {
  return {facts.nativeWidth, presentedMargin(core), facts.vertical};
}

} // namespace spyro
