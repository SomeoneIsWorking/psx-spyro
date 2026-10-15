// guest_terrain_far_vertices.h — the far passes' vertex projection (SCUS_944.25 80028BC0,
// SCUS_942.28 80026274): a low-detail sector's packed vertices, camera-relative, through RTPS into
// scratchpad cells of (screen word, SZ). A clipped sector's screen word is SXY << 5 with outcodes
// below it: 1 above, 2 below, 4 outside the window horizontally, 8 nearer than its near limit.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace spyro::guest_terrain {

struct TerrainFrame;

inline constexpr std::uint32_t kFarCellSize = 8;
inline constexpr std::uint32_t kFarOutcodeShift = 5;

// How an image packs a far vertex's y below its z and x: ten bits, or ten bits doubled.
enum class FarHeight : std::uint8_t { Plain, Doubled };

// A sector's packed vertex words and its camera-relative origin.
struct FarVertices {
  std::uint32_t data = 0;
  std::uint32_t count = 0;
  std::array<std::uint32_t, 3> origin{};
};

// What the sector's list entry says of its screen words.
struct FarClip {
  bool clipped = false; // the words carry outcodes
  bool edge = false;    // the window's are set, and one every vertex shares culls the sector
  std::optional<std::uint32_t> nearBias; // SZ + nearBias < 0 sets the near outcode
};

// Every vertex into the scratchpad; like retail, the loop loads one past the array into V0. False
// when an edge sector's vertices all share an outcode.
[[nodiscard]] bool projectFarVertices(TerrainFrame &frame,
                                      const FarVertices &vertices,
                                      const FarClip &clip,
                                      FarHeight height);

} // namespace spyro::guest_terrain
