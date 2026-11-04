// moby_pose.h — the model-space vertices a moby drawer (SCUS_944.25 80044504, SCUS_944.67 80031124)
// projects for one render-list entry: frame A decoded and blended toward frame B by INTPL.
#pragma once

#include "core.h"

#include <cstdint>
#include <vector>

namespace spyro::moby_pose {

struct Vertex {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

// The first `count` vertices of `entry`'s pose; `deltaTable` is the image's byte-code delta words
// (SCUS_944.25 0x80062674, SCUS_944.67 0x80066274).
[[nodiscard]] std::vector<Vertex>
entryPose(Core &core, std::uint32_t deltaTable, std::uint32_t entry, std::uint32_t count);

} // namespace spyro::moby_pose
