#include "render/field/keyframe_blend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

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

// A progress byte steps a moby under four frames a tick.
constexpr std::uint32_t kMostFramesPast = 3;

std::uint32_t slotA(const Pose &pose) {
  return pose.keyframes & 0xFFu;
}
std::uint32_t slotB(const Pose &pose) {
  return (pose.keyframes >> 8) & 0xFFu;
}
std::uint32_t frameA(const Pose &pose) {
  return (pose.keyframes >> 16) & 0xFFu;
}
std::uint32_t frameB(const Pose &pose) {
  return pose.keyframes >> 24;
}

// The whole frames of `from`'s animation between its frame B and `to`'s frame A: 0 when `to`
// starts at `from`'s frame B, more when the animation stepped past it within the tick.
std::optional<std::uint32_t> framesPast(const Pose &from, const Pose &to) {
  if (slotA(to) != slotB(from)) {
    return std::nullopt;
  }
  if (frameA(to) == frameB(from)) {
    return 0u;
  }
  if (slotA(from) != slotB(from) || frameB(from) != frameA(from) + 1 || frameA(to) < frameB(from) ||
      frameA(to) - frameB(from) > kMostFramesPast) {
    return std::nullopt;
  }
  return frameA(to) - frameB(from);
}

bool samePair(const Pose &from, const Pose &to, Progress encoding) {
  return from.keyframes == to.keyframes &&
         steps(from.progress, encoding) == steps(to.progress, encoding) &&
         step(to.progress, encoding) >= step(from.progress, encoding);
}

// One end's bytes (0 or 1, and 2 or 3): 0 is A, 1 is B.
std::array<std::uint32_t, 2> end(const Pose &pose, std::uint32_t which) {
  return {(pose.keyframes >> (8 * which)) & 0xFFu, (pose.keyframes >> (16 + 8 * which)) & 0xFFu};
}

// The nearer end: A before half way, B from it.
std::array<std::uint32_t, 2> nearer(const Pose &pose, Progress encoding) {
  return end(pose, share(pose, encoding) < 0.5 ? 0u : 1u);
}

} // namespace

bool continues(const Pose &from, const Pose &to, Progress progress) {
  return samePair(from, to, progress) || framesPast(from, to).has_value();
}

Pose between(const Pose &from, const Pose &to, float t, Progress progress) {
  const bool inPair = samePair(from, to, progress);
  const std::optional<std::uint32_t> past = framesPast(from, to);
  if (!inPair && !past) {
    return to;
  }
  // The frames the tick stepped past `from`'s frame B before `to`'s frame A; -1 inside one pair.
  const double skipped = inPair ? -1.0 : static_cast<double>(past.value_or(0));
  const double shown =
      share(from, progress) + (1.0 + skipped + share(to, progress) - share(from, progress)) * t;
  // The pair starting `frames` frames past `from`'s frame A: a pair of `from`'s animation the tick
  // stepped through, at `to`'s step count, until `to`'s own.
  const auto starting = [&](std::uint32_t frames) {
    const std::uint32_t first = frameA(from) + frames;
    if (static_cast<double>(frames) > skipped) {
      return to;
    }
    return Pose{.keyframes = slotA(from) | (slotA(from) << 8) | (first << 16) | ((first + 1) << 24),
                .progress = to.progress};
  };
  const auto whole = static_cast<std::uint32_t>(std::max(0.0, shown));
  const Pose pair = whole == 0 ? from : starting(whole);
  const std::uint32_t k = nearestStep(pair, shown - whole, progress);
  if (k < steps(pair.progress, progress)) {
    return at(pair, k, progress);
  }
  if (whole == 0 || static_cast<double>(whole) <= skipped) {
    // Rounded onto the pair's frame B, the next pair's frame A.
    return at(starting(whole + 1), 0, progress);
  }
  return at(to, steps(to.progress, progress) - 1, progress);
}

Pose across(const Pose &from, const Pose &to, float t, Progress progress) {
  const std::array<std::uint32_t, 2> a = end(from, 0);
  const std::array<std::uint32_t, 2> b = nearer(to, progress);
  // `from` already lies its share of the way off frame A, toward a frame B the switch replaced.
  const double fromShare = share(from, progress);
  const double shown = fromShare + (1.0 - fromShare) * static_cast<double>(t);
  const std::uint32_t k =
      std::min(nearestStep(to, shown, progress), steps(to.progress, progress) - 1);
  return at(
      Pose{.keyframes = a[0] | (b[0] << 8) | (a[1] << 16) | (b[1] << 24), .progress = to.progress},
      k,
      progress);
}

} // namespace spyro::keyframe_blend
