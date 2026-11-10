// keyframe_blend.h — an animated mesh's pose between two fields. The guest draws frame A blended
// toward frame B by the share its progress byte names. Spyro 2 and 3 (the walk 80043EFC, Spyro's
// drawer 80041E30) pack it in nibbles: high nibble r for n = r + 1 steps (16 when r is 0), low
// nibble the step k. Spyro 1 counts k of a fixed n: 64 for a moby (weight k << 6, 0x8001F158),
// 16 for each of Spyro's parts (weight k << 8, 0x80023AC4).
#pragma once

#include <cstdint>
#include <optional>

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

// How the stepper wraps an animation's frames: past `end - 1` its next frame is `start`.
struct Loop {
  std::uint32_t start = 0;
  std::uint32_t end = 0;
};

// `to` is `from`'s pair further on, the pair after it, or a pair of `from`'s animation further
// round `loop` (the tick stepped through whole frames, across the loop's end too). Without a loop
// the frames only count up.
[[nodiscard]] bool continues(const Pose &from,
                             const Pose &to,
                             Progress progress = kNibbleSteps,
                             std::optional<Loop> loop = std::nullopt);

// The pose at `t` along that path: through each pair of `from`'s animation the tick stepped past
// into `to`'s, at the step nearest `t` of the way; `to` when `to` does not continue `from`.
[[nodiscard]] Pose between(const Pose &from,
                           const Pose &to,
                           float t,
                           Progress progress = kNibbleSteps,
                           std::optional<Loop> loop = std::nullopt);

// Across an animation switch, which `to` does not continue: `from`'s frame A as A, `to`'s nearer
// frame as B, at `from`'s share carried `t` of the rest of the way, in `to`'s step count.
[[nodiscard]] Pose across(const Pose &from, const Pose &to, float t, Progress progress);

} // namespace spyro::keyframe_blend
