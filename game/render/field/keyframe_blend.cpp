#include "render/field/keyframe_blend.h"

#include <algorithm>
#include <cmath>

namespace spyro::keyframe_blend {
namespace {

std::uint32_t steps(std::uint8_t progress) {
  const std::uint32_t row = progress >> 4;
  return row == 0 ? 16 : row + 1;
}

std::uint32_t step(std::uint8_t progress) {
  return progress & 0xFu;
}

// The step nearest `share` (0 to 1) of the way from `pose`'s frame A to B; `n` is frame B itself.
std::uint32_t nearestStep(const Pose &pose, double share) {
  return static_cast<std::uint32_t>(std::lround(std::max(0.0, share) * steps(pose.progress)));
}

Pose at(const Pose &pose, std::uint32_t k) {
  return Pose{.keyframes = pose.keyframes,
              .progress = static_cast<std::uint8_t>((pose.progress & 0xF0u) | k)};
}

double share(const Pose &pose) {
  return static_cast<double>(step(pose.progress)) / steps(pose.progress);
}

// `to`'s frame A is `from`'s frame B.
bool advanced(const Pose &from, const Pose &to) {
  return (to.keyframes & 0xFFu) == ((from.keyframes >> 8) & 0xFFu) &&
         ((to.keyframes >> 16) & 0xFFu) == (from.keyframes >> 24);
}

} // namespace

Pose between(const Pose &from, const Pose &to, float t) {
  const bool same = from.keyframes == to.keyframes && steps(from.progress) == steps(to.progress) &&
                    step(to.progress) >= step(from.progress);
  if (!same && !advanced(from, to)) {
    return to;
  }
  const double shown = share(from) + ((same ? 0.0 : 1.0) + share(to) - share(from)) * t;
  const std::uint32_t inFrom = nearestStep(from, shown);
  if (inFrom < steps(from.progress)) {
    return at(from, inFrom);
  }
  return at(to, std::min(nearestStep(to, shown - 1.0), steps(to.progress) - 1));
}

} // namespace spyro::keyframe_blend
