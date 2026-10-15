// guest_view_slope.h — the horizontal frustum slope, screen margin and outcode window the
// presentation plan asks of the guests' culling tests: retail's at 4:3, widened by the plan at
// 16:9.
#pragma once

#include "core.h"
#include "guest_moby_frustum.h"
#include "guest_terrain_facts.h"

namespace spyro {

[[nodiscard]] guest_moby_frustum::HorizontalSlope presentedSlope(const Core &core);

// The columns the presentation shows past each side of the authored window, 0 at 4:3.
[[nodiscard]] int presentedMargin(const Core &core);

// The outcode window of a drawer with `facts`, widened by the presented margin.
[[nodiscard]] guest_terrain::ScreenBounds presentedBounds(const Core &core,
                                                          const guest_terrain::Facts &facts);

} // namespace spyro
