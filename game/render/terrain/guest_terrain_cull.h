// guest_terrain_cull.h — the polygon culls the terrain passes of this engine family share: the
// outcode reject of a clipped sector's screen words, and the NCLIP winding tests.
//
// A clipped sector's screen word is SXY << 5 with outcodes in its low bits (guest_terrain_mesh.h);
// a polygon whose corners share one is off screen. The winding tests run NCLIP on the corners as
// retail does, so the GTE is left as the image leaves it.
#pragma once

#include "guest_terrain_mesh.h"
#include "guest_word.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace spyro::guest_terrain {

struct TerrainFrame;

// A near polygon's screen words, shifted off the clip grid, and the overflow bits they carried.
template <std::size_t N> struct NearCorners {
  std::array<std::uint32_t, N> xy{};
  std::uint32_t overflow = 0;
};

// Nullopt when every corner of a clipped polygon shares an outcode. `keepsOverflow` is false for a
// pass whose words carry no overflow bits.
template <std::size_t N>
[[nodiscard]] std::optional<NearCorners<N>>
unclipCorners(const std::array<std::uint32_t, N> &xy, bool clipped, bool keepsOverflow) {
  NearCorners<N> corners{.xy = xy};
  if (!clipped) {
    return corners;
  }
  std::uint32_t shared = 0xFFFFFFFFu;
  std::uint32_t any = 0;
  for (const std::uint32_t word : xy) {
    shared &= word;
    any |= word;
  }
  if ((shared & 0xFu) != 0) {
    return std::nullopt;
  }
  if (keepsOverflow) {
    corners.overflow = any & kOverflowOutcode;
  }
  for (std::uint32_t &word : corners.xy) {
    word = guest_word::sra(word, 5);
  }
  return corners;
}

// A polygon's draw-word sides, whose bits differ by image.
struct Sidedness {
  bool twoSided = false;
  bool flipped = false;
};

// The second triangle a back-facing split piece is tested on: the fourth corner in the first's
// place (SCUS_944.25 800264B0, facing when the area is not positive), or (fourth, third, second)
// (SCUS_942.28 80027DF4, facing when it is not negative).
enum class SecondTriangle : std::uint8_t { FourthFirst, Reversed };

// The backface test a subdivision pass gives a piece of an overflowed polygon, whose whole the near
// pass could not test: two-sided pieces pass; otherwise the (first, second, third) winding decides,
// and a back-facing one is given a second chance on `second`.
[[nodiscard]] bool facesCamera(TerrainFrame &frame,
                               const std::array<std::uint32_t, 4> &screens,
                               Sidedness sides,
                               SecondTriangle second);
[[nodiscard]] bool triangleFacesCamera(TerrainFrame &frame,
                                       const std::array<std::uint32_t, 3> &screens,
                                       Sidedness sides);

// The near passes' quad test (SCUS_944.25 80024A28, SCUS_942.28 80026BA4) on (first, second,
// fourth, third): (first, second, fourth) decides, and a back face is retried with the third in
// the first's place, which a two-sided quad still takes. An overflowed corner skips the winding.
[[nodiscard]] bool nearQuadFacesCamera(TerrainFrame &frame,
                                       const std::array<std::uint32_t, 4> &screens,
                                       Sidedness sides,
                                       bool overflow);
// The near passes' triangle test (SCUS_944.25 80024F8C, SCUS_942.28 80027168): NCLIP, and a
// one-sided triangle wound away is culled unless a corner overflowed.
[[nodiscard]] bool nearTriangleFacesCamera(TerrainFrame &frame,
                                           const std::array<std::uint32_t, 3> &screens,
                                           Sidedness sides,
                                           bool overflow);

} // namespace spyro::guest_terrain
