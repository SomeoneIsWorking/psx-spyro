// moby_pose.h — the model-space vertices a moby drawer projects for one render-list entry: frame A's
// vertex streams decoded and, when the entry names a frame B share, blended toward frame B's.
//
// The drawer (SCUS_944.25 80044504, SCUS_944.67 80031124, one body) reads each frame as a byte
// stream of indices into a delta table, a halfword stream of short deltas and absolute vertices, and
// the mesh's per-vertex base deltas; bit 0 of each code picks the stream the next vertex comes from.
// It blends with INTPL by the share in the entry's class word and projects (z, x, y) of the blend as
// VZ0, VXY0.
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
