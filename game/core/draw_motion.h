// draw_motion.h — what the Spyro 2 / Spyro 3 draw body reads that moves between logic frames
// (docs/plans/spyro23-replay.md), as motion tracks for its replay: the camera, Spyro, his poses
// and shadow, every moby slot, and the effects pass's pools.
#pragma once

#include "motion_set.h"
#include "render/field/effect_pools.h"
#include "render/field/guest_moby_visibility.h"

#include <array>
#include <cstdint>
#include <span>

class Core;

namespace spyro {

struct DrawMotionFacts {
  std::uint32_t cameraAngles = 0;   // three 12-bit angle halfwords
  std::uint32_t cameraPosition = 0; // x, y, z words
  // Spyro's record: position words, body and head angle bytes (+0xC, +0x10), body and head
  // keyframes (+0x14, +0x18) and their nibble progress bytes (+0x1C, +0x1D).
  std::uint32_t spyro = 0;
  // His shadow: position words, then three angle bytes and a flag byte.
  std::uint32_t shadow = 0;
  const guest_moby::Facts *moby = nullptr;
  effect_pools::Facts effects;
};

// Spyro's body and head poses: each part's keyframe pair and nibble progress.
class SpyroPoses final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The moby slot words MobyMotion blends, in its order: position, keyframes, progress, angle
// bytes, flags (angle fractions), and the mesh halfword it pairs by.
inline constexpr std::array<psx::present::MotionField, 8> kMobyFields{{
    {0x0Cu, psx::present::Motion::Step},
    {0x10u, psx::present::Motion::Step},
    {0x14u, psx::present::Motion::Step},
    {0x3Cu, psx::present::Motion::Step},
    {0x40u, psx::present::Motion::Step},
    {0x44u, psx::present::Motion::Step},
    {0x54u, psx::present::Motion::Step},
    {0x34u, psx::present::Motion::Step},
}};

// A moby slot: position, pose (across an animation switch when both frames share the mesh's
// vertex layout) and angle bytes, refined by the flag word's fractions when both samples carry
// them.
class MobyMotion final : public psx::present::MotionRule {
public:
  MobyMotion(Core &core, const guest_moby::Facts &walk) : core_(core), walk_(walk) {}
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;

private:
  Core &core_;
  const guest_moby::Facts &walk_;
};

// The effects pass's pools, paired by effect_pools' own rules.
class EffectMotion final : public psx::present::MotionSource {
public:
  explicit EffectMotion(const effect_pools::Facts &facts) : facts_(facts) {}
  void sample(Core &core) override;
  void write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  const effect_pools::Facts &facts_;
  std::array<effect_pools::Records, 2> records_{};
  std::size_t later_ = 0;
};

class DrawMotion {
public:
  DrawMotion(Core &core, const DrawMotionFacts &facts);

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }
  [[nodiscard]] std::span<psx::present::MotionSource *const> sources() const {
    return sources_;
  }

private:
  using Field = psx::present::MotionField;
  static constexpr psx::present::Motion kLinear = psx::present::Motion::Linear;
  static constexpr psx::present::Motion kStep = psx::present::Motion::Step;
  static constexpr psx::present::Motion kAngle8 = psx::present::Motion::Angle8x3;

  static constexpr std::array<Field, 2> kCameraAngles{{
      {0x00u, psx::present::Motion::Angle12x2},
      {0x04u, psx::present::Motion::Angle12Low},
  }};
  static constexpr std::array<Field, 3> kVector{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
  }};
  static constexpr std::array<Field, 5> kSpyro{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x0Cu, kAngle8},
      {0x10u, kAngle8},
  }};
  // SpyroPoses' words, in its order.
  static constexpr std::array<Field, 3> kSpyroPoses{{
      {0x14u, kStep},
      {0x18u, kStep},
      {0x1Cu, kStep},
  }};
  // Position words, then three angle bytes whose top byte is the hidden flag.
  static constexpr std::array<Field, 4> kShadow{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x0Cu, kAngle8},
  }};
  // [moby array]: 0x58-byte slots up to the first whose flag (+0x48) is 0xFF; a negative flag
  // skips. One object while the mesh (+0x36) holds.
  static constexpr std::array<psx::present::MotionKey, 1> kMobyKeys{{{0x34u, 0xFFFF0000u}}};
  static constexpr psx::present::MotionRecords kMobyRecords{
      .throughPointer = true,
      .stride = 0x58u,
      .count = 0x800u,
      .end = psx::present::RecordByte{0x48u, 0xFFu, 0xFFu},
      .live = psx::present::RecordByte{0x48u, 0x00u, 0x7Fu}};

  SpyroPoses poses_;
  MobyMotion moby_;
  EffectMotion effects_;
  std::array<psx::present::MotionTrack, 6> tracks_;
  std::array<psx::present::MotionSource *, 1> sources_{&effects_};
};

} // namespace spyro
