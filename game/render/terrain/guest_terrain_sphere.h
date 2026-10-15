// guest_terrain_sphere.h — the bounding-sphere test the classification pass of this engine family
// gives each sector (SCUS_944.25 80023CB0, SCUS_942.28 80025A30): its centre, camera-relative,
// rotated by the classification matrix (MVMVA RT * IR), then tested against the four frustum
// planes with retail's shift-sum margins in the image's wrapping 32-bit arithmetic.
//
// Only the horizontal "outside" plane takes a slope: at 16:9 it is the widened one, so sectors in
// the extra columns are drawn; the "inside" test stays retail's, which only sends more sectors
// through the outcode tests.
#pragma once

#include "core.h"
#include "render/field/guest_moby_frustum.h"

#include <array>
#include <cstdint>

namespace spyro::guest_terrain {

struct SectorSphere {
  bool visible = false;
  bool inside = false; // wholly inside every plane: its polygons need no edge tests
  std::uint32_t z = 0; // the centre's view depth (IR3)
};

// The camera position in the units sector centres are stored in (position >> 4).
struct SphereCamera {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

// A sector's first word (x high, y low), its second (z high), and the radius in the second.
struct SphereWords {
  std::uint32_t centreXY = 0;
  std::uint32_t centreZ = 0;
  std::uint32_t radius = 0;
};

[[nodiscard]] SectorSphere testSectorSphere(Core &core,
                                            const SphereWords &words,
                                            const SphereCamera &camera,
                                            const guest_moby_frustum::HorizontalSlope &slope);

} // namespace spyro::guest_terrain
