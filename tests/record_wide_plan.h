// The plan the Record presenter latches for a 512x240 display at 16:9.
#pragma once

#include "guest_widescreen_projection.h"

namespace spyro::test {

inline GuestProjectionPlan recordWidePlan() {
  return psx::present::guestProjectionPlan({
      .path = RenderPath::Record,
      .requested = PresentationAspect::Wide16x9,
      .nativePresentation = {512, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {1920, 1080},
      .vramWidth = 1024,
  });
}

} // namespace spyro::test
