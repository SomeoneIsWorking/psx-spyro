// keyframe_blend.h — an animated mesh's pose between two fields. The guest draws frame A blended
// toward frame B by the share its progress byte names. Spyro 2 and 3 (the walk 80043EFC, Spyro's
// drawer 80041E30) pack it in nibbles: high nibble r for n = r + 1 steps (16 when r is 0), low
// nibble the step k. Spyro 1 counts k of a fixed n: 64 for a moby (weight k << 6, 0x8001F158),
// 16 for each of Spyro's parts (weight k << 8, 0x80023AC4).
#pragma once

#include <cstdint>

namespace spyro::keyframe_blend {

struct Pose {
  std::uint32_t keyframes = 0; // bytes: slot A, slot B, frame A, frame B
  std::uint8_t progress = 0;
};

// The steps a progress byte counts; 0 for the nibble-packed form.
struct Progress {
  std::uint32_t fixedSteps = 0;
};

inline constexpr Progress kNibbleSteps{};

// The pose at `t`: in `from`'s pair or, when `to`'s frame A is `from`'s frame B, in `to`'s, at the
// step nearest `t` of the way; `to` when the two are not one animation a step apart.
[[nodiscard]] Pose
between(const Pose &from, const Pose &to, float t, Progress progress = kNibbleSteps);

} // namespace spyro::keyframe_blend
