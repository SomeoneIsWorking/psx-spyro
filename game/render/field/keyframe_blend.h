// keyframe_blend.h — an animated mesh's pose between two fields. The guest draws frame A blended
// toward frame B by the share its progress byte names (the walk 80043EFC, Spyro's drawer
// 80041E30): high nibble r for n = r + 1 steps (16 when r is 0), low nibble the step k, share k/n.
#pragma once

#include <cstdint>

namespace spyro::keyframe_blend {

struct Pose {
  std::uint32_t keyframes = 0; // bytes: slot A, slot B, frame A, frame B
  std::uint8_t progress = 0;
};

// The pose at `t`: in `from`'s pair or, when `to`'s frame A is `from`'s frame B, in `to`'s, at the
// step nearest `t` of the way; `to` when the two are not one animation a step apart.
[[nodiscard]] Pose between(const Pose &from, const Pose &to, float t);

} // namespace spyro::keyframe_blend
