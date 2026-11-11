#include "spyro1/core/frame/draw_motion.h"

#include "blend.h"
#include "core.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_record.h"
#include "render/field/record_blend.h"
#include "render/terrain/guest_camera_builder.h"
#include "spyro1/render/actor_emitter_layout.h"
#include "spyro1/render/actor_facts.h"

#include <array>
#include <cmath>
#include <cstring>
#include <optional>

namespace spyro1 {
namespace {

namespace blend = psx::present;
using spyro::record_blend::angleBetween;
using spyro::record_blend::byteBetween;

// Byte offsets inside the 16 pose bytes from g_Spyro +0x18: part p's anim pair at 2p, frame pair at
// 6 + 2p, progress at 12 + p (0x80023AC4).
constexpr std::size_t kPoseBytes = 16;
constexpr std::size_t kPoseParts = 3;
constexpr std::size_t kFrameBytes = 6;
constexpr std::size_t kProgressBytes = 12;
constexpr spyro::keyframe_blend::Progress kPartSteps{16};
// Spyro's animation table: four bytes per animation, its loop start then its end; 0x8003CB24
// steps a part's next frame past the end back to the start.
constexpr std::uint32_t kSpyroAnimations = 0x8006C4A0u;
constexpr std::uint32_t kSpyroAnimationBytes = 4u;

template <std::size_t N> std::array<std::uint8_t, N> bytesOf(std::span<const std::uint32_t> words) {
  std::array<std::uint8_t, N> bytes{};
  std::memcpy(bytes.data(), words.data(), N);
  return bytes;
}

template <std::size_t N>
void store(const std::array<std::uint8_t, N> &bytes, std::span<std::uint32_t> out) {
  std::memcpy(out.data(), bytes.data(), N);
}

std::uint8_t byteOf(std::uint32_t word, unsigned index) {
  return static_cast<std::uint8_t>(word >> (8u * index));
}

// A byte of a word, 0 the lowest.
struct Lane {
  unsigned index = 0;
};

std::uint32_t withByte(std::uint32_t word, Lane lane, std::uint8_t byte) {
  const unsigned shift = 8u * lane.index;
  return (word & ~(0xFFu << shift)) | (std::uint32_t{byte} << shift);
}

// A halfword the short way round 16 bits, whether the guest reads it signed or not.
std::uint32_t turn16(std::uint32_t from, std::uint32_t to, unsigned shift, float t) {
  return blend::lerpTurn(from >> shift, to >> shift, 16, t) << shift;
}

spyro::keyframe_blend::Pose partOf(const std::array<std::uint8_t, kPoseBytes> &bytes,
                                   std::size_t part) {
  const std::size_t pair = 2 * part;
  const std::size_t frames = kFrameBytes + pair;
  const auto byte = [&bytes](std::size_t at, unsigned shift) {
    return static_cast<std::uint32_t>(bytes[at]) << shift;
  };
  return {.keyframes = byte(pair, 0) | byte(pair + 1, 8) | byte(frames, 16) | byte(frames + 1, 24),
          .progress = bytes[kProgressBytes + part]};
}

constexpr std::uint32_t kMobyShadow = 0x1Cu;
constexpr std::uint32_t kMobyScale = 0x57u;

// +0x1C: ground distance in the low halfword (0 when there is no ground below), two 64-step tilts
// at bits 16 and 22; the sign bit stages the shadow (0x8001F158, 0x80059F8C).
std::uint32_t shadowBetween(std::uint32_t from, std::uint32_t to, float t) {
  if (((from ^ to) & 0x80000000u) != 0) {
    return to;
  }
  const std::uint32_t distance = (from & 0xFFFFu) != 0 && (to & 0xFFFFu) != 0
                                     ? blend::lerpHalf(from, to, blend::Halfword::Low, t)
                                     : to & 0xFFFFu;
  const std::uint32_t tiltLow = blend::lerpTurn(from >> 16, to >> 16, 6, t);
  const std::uint32_t tiltHigh = blend::lerpTurn(from >> 22, to >> 22, 6, t);
  return (to & 0xF0000000u) | tiltHigh << 22 | tiltLow << 16 | distance;
}

// Particle kinds (0x800573C8): 1 and 4 are lines, their second point at +0x0A; 2, 5 and 6 on are
// turned by the angle byte at +0x0B.
bool isLine(std::uint8_t kind) {
  return kind == 1 || kind == 4;
}
bool isTurned(std::uint8_t kind) {
  return kind == 2 || kind == 5 || kind >= 6;
}

// m_Rotation's halfwords: a 12-bit angle with whole turns above it (x is stored unmasked).
constexpr unsigned kTurnBits = 12;
constexpr std::uint32_t kHalfword = 0xFFFFu;

// One camera angle `t` of the way the short way round, in Angles' fine unit.
std::int32_t fineAngle(std::uint32_t from, std::uint32_t to, float t) {
  constexpr double kFine = 1 << spyro::guest_camera::kFineBits;
  return static_cast<std::int32_t>(
      std::nearbyint(blend::turnedBetween(from, to, kTurnBits, t) * kFine));
}

} // namespace

void CameraMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  const std::span<const std::uint32_t> from = samples.from;
  const std::span<const std::uint32_t> to = samples.to;
  for (std::size_t word = 0; word < 3; ++word) {
    out[word] = blend::lerpWord(from[word], to[word], t);
  }
  out[3] = blend::lerpTurned(from[3], to[3], kTurnBits, t) |
           blend::lerpTurned(from[3] >> 16, to[3] >> 16, kTurnBits, t) << 16;
  out[4] = (to[4] & ~kHalfword) | blend::lerpTurned(from[4], to[4], kTurnBits, t);
  const spyro::guest_camera::Angles angles{.aroundX = fineAngle(from[3], to[3], t),
                                           .aroundY = fineAngle(from[3] >> 16, to[3] >> 16, t),
                                           .aroundZ = fineAngle(from[4], to[4], t)};
  // Whole angles take the guest's own matrices.
  constexpr std::int32_t kFineMask = (1 << spyro::guest_camera::kFineBits) - 1;
  if (((angles.aroundX | angles.aroundY | angles.aroundZ) & kFineMask) == 0) {
    noted_.reset();
    return;
  }
  noted_ = angles;
}

void SpyroPoses::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  const auto earlier = bytesOf<kPoseBytes>(samples.from);
  auto shown = bytesOf<kPoseBytes>(samples.to);
  for (std::size_t part = 0; part < kPoseParts; ++part) {
    const spyro::keyframe_blend::Pose from = partOf(earlier, part);
    const std::uint32_t row = kSpyroAnimations + (from.keyframes & 0xFFu) * kSpyroAnimationBytes;
    const spyro::keyframe_blend::Loop loop{.start = core_.mem_r8(row),
                                           .end = core_.mem_r8(row + 1u)};
    const spyro::keyframe_blend::Pose pose =
        spyro::keyframe_blend::between(from,
                                       partOf(shown, part),
                                       t,
                                       kPartSteps,
                                       loop.end > loop.start ? std::optional(loop) : std::nullopt);
    const std::size_t pair = 2 * part;
    const std::size_t frames = kFrameBytes + pair;
    shown[pair] = static_cast<std::uint8_t>(pose.keyframes);
    shown[pair + 1] = static_cast<std::uint8_t>(pose.keyframes >> 8u);
    shown[frames] = static_cast<std::uint8_t>(pose.keyframes >> 16u);
    shown[frames + 1] = static_cast<std::uint8_t>(pose.keyframes >> 24u);
    shown[kProgressBytes + part] = pose.progress;
  }
  store(shown, out);
}

void MobyMotion::finish(const Samples &samples,
                        const spyro::moby_record::Bytes &from,
                        const spyro::moby_record::Bytes &to,
                        const spyro::moby_record::Between &moved,
                        float t,
                        spyro::moby_record::Bytes &shown) const {
  using spyro::record_blend::load;
  using spyro::record_blend::store;
  store(shown,
        kMobyShadow,
        shadowBetween(
            load<std::uint32_t>(from, kMobyShadow), load<std::uint32_t>(to, kMobyShadow), t));
  angles_.note(samples.address, moved.angleFractions);
  // +0x57 m_ScaleOverride: 0 draws the regular size, so only two overrides blend.
  if (from[kMobyScale] != 0 && to[kMobyScale] != 0) {
    shown[kMobyScale] = byteBetween(from[kMobyScale], to[kMobyScale], t);
  }
}

std::optional<spyro::keyframe_blend::Loop> MobyMotion::loopOf(std::uint32_t mesh,
                                                              std::uint32_t animation) const {
  const std::uint32_t header = core_.mem_r32(core_.mem_r32(guest::kModels + mesh * 4) +
                                             actors::mesh::kFrames + animation * 4);
  const auto frames = static_cast<std::int16_t>(core_.mem_r16(header + actors::mesh::kFrameCount));
  if (frames <= 0) {
    return std::nullopt;
  }
  return spyro::keyframe_blend::Loop{.start = 0, .end = static_cast<std::uint32_t>(frames)};
}

bool MobyMotion::sharesLayout(std::uint32_t mesh, const spyro::keyframe_blend::Pose &pose) const {
  const std::uint32_t model = core_.mem_r32(guest::kModels + mesh * 4);
  const auto frame = [&](unsigned shift) {
    return core_.mem_r32(model + actors::mesh::kFrames + ((pose.keyframes >> shift) & 0xFFu) * 4);
  };
  const std::uint32_t a = frame(0);
  const std::uint32_t b = frame(8);
  namespace layout = actors::emitter::frame;
  return core_.mem_r16(a + layout::kVertexCounts) == core_.mem_r16(b + layout::kVertexCounts) &&
         core_.mem_r8(a + layout::kPositionShift) == core_.mem_r8(b + layout::kPositionShift) &&
         core_.mem_r8(a + layout::kHalfShift) == core_.mem_r8(b + layout::kHalfShift);
}

void ParticleMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  std::copy(samples.to.begin(), samples.to.end(), out.begin());
  const std::span<const std::uint32_t> from = samples.from;
  const std::span<const std::uint32_t> to = samples.to;
  const std::uint8_t kind = byteOf(to[0], 1);
  out[1] = turn16(from[1], to[1], 0, t) | turn16(from[1], to[1], 16, t);
  out[2] = (to[2] & 0xFFFF0000u) | turn16(from[2], to[2], 0, t);
  if (isLine(kind)) {
    out[2] = (out[2] & 0xFFFFu) | turn16(from[2], to[2], 16, t);
    out[3] = turn16(from[3], to[3], 0, t) | turn16(from[3], to[3], 16, t);
  } else if (isTurned(kind)) {
    out[2] = withByte(out[2], Lane{3}, angleBetween(byteOf(from[2], 3), byteOf(to[2], 3), t));
  }
}

void SparkleMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  std::copy(samples.to.begin(), samples.to.end(), out.begin());
  // +0x0C fade, +0x0D full fade, +0x0E angle, +0x0F turn.
  const std::uint8_t fadeFrom = byteOf(samples.from[3], 0);
  const std::uint8_t fadeTo = byteOf(samples.to[3], 0);
  if (fadeTo > fadeFrom) {
    return;
  }
  for (std::size_t word = 0; word < 3; ++word) {
    out[word] = blend::lerpWord(samples.from[word], samples.to[word], t);
  }
  std::uint32_t timing = withByte(samples.to[3], Lane{0}, byteBetween(fadeFrom, fadeTo, t));
  out[3] = withByte(
      timing, Lane{2}, angleBetween(byteOf(samples.from[3], 2), byteOf(samples.to[3], 2), t));
}

void FlameMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  std::copy(samples.to.begin(), samples.to.end(), out.begin());
  for (std::size_t word = 0; word < 3; ++word) {
    out[word] = blend::lerpWord(samples.from[word], samples.to[word], t);
  }
  // Words 3..4 are the eight signed part lengths, 5..6 each part's limit.
  for (unsigned part = 0; part < 8; ++part) {
    const std::size_t lengths = 3 + part / 4;
    const std::size_t limits = 5 + part / 4;
    const unsigned index = part % 4;
    const auto before = static_cast<std::int8_t>(byteOf(samples.from[lengths], index));
    const auto after = static_cast<std::int8_t>(byteOf(samples.to[lengths], index));
    if (before > 0 && after > before &&
        byteOf(samples.from[limits], index) == byteOf(samples.to[limits], index)) {
      out[lengths] = withByte(
          out[lengths],
          Lane{index},
          byteBetween(static_cast<std::uint8_t>(before), static_cast<std::uint8_t>(after), t));
    }
  }
}

} // namespace spyro1
