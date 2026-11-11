// spyro_drawer_depth.h — the S2/S3 player drawer's depth: its view unit and sort bias for each
// call, and the calls whose faces draw in slot order (the reflection pass, a pass chained after the
// whole frame).
#pragma once

#include "render/field/guest_render_globals.h"

#include <cstdint>
#include <span>

namespace spyro::spyro_drawer_depth {

struct Facts {
  // The drawer (SCUS_944.25 0x80041DBC, SCUS_944.67 0x8003CDA0).
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // Spyro's record: the drawer reads its signed depth-bias byte at +0x1E and mirrors him below the
  // water plane while the reflection shift +0x2C is set.
  std::uint32_t spyro = 0;
  // Calls, by return address, whose pass the frame chains after every bin.
  std::span<const std::uint32_t> unbinnedReturns;
};

// Each face is sorted by its four SZ summed, 64 of that sum to a bucket (80042C7C): a bucket in
// near view units.
inline constexpr float kBucketViewUnits = 64.0f / 4.0f * guest_render_globals::kWorldDepthScale;

// The drawer's projections in near view units (its translation is world minus camera), nearer by
// his bias byte in bins; a reflection or unbinned pass projects with no depth, so it draws in slot
// order and tests nothing.
void draw(Core &core, const Facts &facts);

// Marks the drawer's faces as a per-face sorted mesh.
void markDepth(Core &core, const Facts &facts);

} // namespace spyro::spyro_drawer_depth
