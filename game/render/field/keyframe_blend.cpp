#include "render/field/keyframe_blend.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace spyro::keyframe_blend {
namespace {

std::uint32_t steps(std::uint8_t progress, Progress encoding) {
  if (encoding.fixedSteps != 0) {
    return encoding.fixedSteps;
  }
  const std::uint32_t row = progress >> 4;
  return row == 0 ? 16 : row + 1;
}

std::uint32_t step(std::uint8_t progress, Progress encoding) {
  return encoding.fixedSteps != 0 ? progress : progress & 0xFu;
}

// The step nearest `share` (0 to 1) of the way from `pose`'s frame A to B; `n` is frame B itself.
std::uint32_t nearestStep(const Pose &pose, double share, Progress encoding) {
  return static_cast<std::uint32_t>(
      std::lround(std::max(0.0, share) * steps(pose.progress, encoding)));
}

Pose at(const Pose &pose, std::uint32_t k, Progress encoding) {
  const std::uint32_t kept = encoding.fixedSteps != 0 ? 0u : pose.progress & 0xF0u;
  return Pose{.keyframes = pose.keyframes, .progress = static_cast<std::uint8_t>(kept | k)};
}

double share(const Pose &pose, Progress encoding) {
  return static_cast<double>(step(pose.progress, encoding)) / steps(pose.progress, encoding);
}

// `to`'s frame A is `from`'s frame B.
bool advanced(const Pose &from, const Pose &to) {
  return (to.keyframes & 0xFFu) == ((from.keyframes >> 8) & 0xFFu) &&
         ((to.keyframes >> 16) & 0xFFu) == (from.keyframes >> 24);
}

bool samePair(const Pose &from, const Pose &to, Progress encoding) {
  return from.keyframes == to.keyframes &&
         steps(from.progress, encoding) == steps(to.progress, encoding) &&
         step(to.progress, encoding) >= step(from.progress, encoding);
}

// The slot and frame bytes of the pair's nearer end: A before half way, B from it.
std::array<std::uint32_t, 2> nearer(const Pose &pose, Progress encoding) {
  const std::uint32_t end = share(pose, encoding) < 0.5 ? 0u : 1u;
  return {(pose.keyframes >> (8 * end)) & 0xFFu, (pose.keyframes >> (16 + 8 * end)) & 0xFFu};
}

} // namespace

bool continues(const Pose &from, const Pose &to, Progress progress) {
  return samePair(from, to, progress) || advanced(from, to);
}

Pose between(const Pose &from, const Pose &to, float t, Progress progress) {
  if (!continues(from, to, progress)) {
    return to;
  }
  const bool inPair = samePair(from, to, progress);
  const double shown = share(from, progress) +
                       ((inPair ? 0.0 : 1.0) + share(to, progress) - share(from, progress)) * t;
  const std::uint32_t inFrom = nearestStep(from, shown, progress);
  if (inFrom < steps(from.progress, progress)) {
    return at(from, inFrom, progress);
  }
  return at(to,
            std::min(nearestStep(to, shown - 1.0, progress), steps(to.progress, progress) - 1),
            progress);
}

Pose across(const Pose &from, const Pose &to, float t, Progress progress) {
  const std::array<std::uint32_t, 2> a = nearer(from, progress);
  const std::array<std::uint32_t, 2> b = nearer(to, progress);
  const auto k = static_cast<std::uint32_t>(
      std::lround(std::clamp(static_cast<double>(t), 0.0, 1.0) * progress.fixedSteps));
  return Pose{.keyframes = a[0] | (b[0] << 8) | (a[1] << 16) | (b[1] << 24),
              .progress = static_cast<std::uint8_t>(k)};
}

} // namespace spyro::keyframe_blend
