// draw_motion.h — what GamestateDraw's GS_Playing section reads that moves between logic frames
// (docs/plans/spyro1-motion-tracks.md), as motion tracks for its replay, and the rules for the
// records that move as a unit.
#pragma once

#include "motion_set.h"
#include "render/field/keyframe_blend.h"
#include "spyro1/core/effect/flame_block.h"
#include "spyro1/core/effect/shadow_block.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/glows.h"
#include "spyro1/render/particles.h"
#include "spyro1/render/replay_angles.h"

#include <array>
#include <cstdint>
#include <span>

namespace spyro1 {

// Spyro's body, head and tail poses (g_Spyro +0x18..+0x27): each part's animation pair and progress
// step one unit, blended as keyframe_blend does for the guest's 16-step weight.
class SpyroPoses final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// A moby slot: position, shadow word, pose (64 steps), angle bytes and scale override. The angle
// bytes are written at the step each lies past, and the sixteenths past it noted for the list
// builders. An animation switch blends across when both frames share a vertex layout in the
// class's mesh (the emitter reads both with frame A's counts and shifts).
class MobyMotion final : public psx::present::MotionRule {
public:
  MobyMotion(Core &core, actors::ReplayAngles &angles) : core_(core), angles_(angles) {}
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;

private:
  // `pose`'s two frames in mesh `mesh`.
  [[nodiscard]] bool sharesLayout(std::uint32_t mesh,
                                  const spyro::keyframe_blend::Pose &pose) const;

  Core &core_;
  actors::ReplayAngles &angles_;
};

// A particle record: its points and, for turned kinds, its angle; a life that rose is a new
// particle in the slot.
class ParticleMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// A sparkle: position, angle, and its fade while it counts down; a rising fade is a new sparkle.
class SparkleMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The flame burst: its anchor, and each part's length while it grows toward an unchanged limit.
class FlameMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

class DrawMotion {
public:
  DrawMotion(Core &core, actors::ReplayAngles &angles) : moby_(core, angles) {}

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }

private:
  using Field = psx::present::MotionField;
  using Key = psx::present::MotionKey;
  using Records = psx::present::MotionRecords;
  using RecordByte = psx::present::RecordByte;
  static constexpr psx::present::Motion kLinear = psx::present::Motion::Linear;
  static constexpr psx::present::Motion kStep = psx::present::Motion::Step;

  // g_Camera: m_Position, then m_Rotation's three 12-bit halfwords.
  static constexpr std::array<Field, 5> kCamera{{
      {0x28u, kLinear},
      {0x2Cu, kLinear},
      {0x30u, kLinear},
      {0x4Cu, psx::present::Motion::Angle12x2},
      {0x50u, psx::present::Motion::Angle12Low},
  }};
  // g_Spyro: m_Position, then the body, head and tail rotations, three 8-bit angles and a pad byte
  // each.
  static constexpr std::array<Field, 6> kSpyro{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x0Cu, psx::present::Motion::Angle8x3},
      {0x10u, psx::present::Motion::Angle8x3},
      {0x14u, psx::present::Motion::Angle8x3},
  }};
  static constexpr std::array<Field, 4> kSpyroPoses{{
      {0x18u, kStep},
      {0x1Cu, kStep},
      {0x20u, kStep},
      {0x24u, kStep},
  }};
  static constexpr std::array<Field, 3> kVector{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
  }};

  // [g_LevelMobys]: 0x58-byte slots up to the first whose state (+0x48) is 0xFF; bit 7 set is
  // dead. One object while the props pointer (+0x00), class (+0x36) and screen-space flag (+0x50
  // bit 7) hold.
  static constexpr std::array<Key, 3> kMobyKeys{{
      {0x00u},
      {0x34u, 0xFFFF0000u},
      {0x50u, 0x80u},
  }};
  static constexpr Records kMobyRecords{.throughPointer = true,
                                        .stride = 0x58u,
                                        .count = 0x800u,
                                        .end = RecordByte{0x48u, 0xFFu, 0xFFu},
                                        .live = RecordByte{0x48u, 0x00u, 0x7Fu}};
  // MobyMotion's words, in its order.
  static constexpr std::array<Field, 9> kMoby{{
      {0x0Cu, kStep},
      {0x10u, kStep},
      {0x14u, kStep},
      {0x1Cu, kStep},
      {0x3Cu, kStep},
      {0x40u, kStep},
      {0x44u, kStep},
      {0x54u, kStep},
      {0x34u, kStep},
  }};

  // g_Hud.m_Mobys[12]: the count and icon mobys slide in y and spin in z; a digit, substate or
  // radius change is another object (HudMobyRotate).
  static constexpr std::uint32_t kHud = 0x80077FA8u;
  static constexpr std::uint32_t kHudMobys = kHud + 0x44u;
  static constexpr std::array<Key, 3> kHudMobyKeys{{
      {0x34u, 0xFFFF0000u},
      {0x48u, 0xFF00u},
      {0x50u, 0xFFu},
  }};
  static constexpr Records kHudMobyRecords{.stride = 0x58u, .count = 12u};
  static constexpr std::array<Field, 4> kHudMoby{{
      {0x0Cu, kLinear},
      {0x10u, kLinear},
      {0x14u, kLinear},
      {0x44u, psx::present::Motion::Angle8x3},
  }};
  // g_Hud.m_SpriteRect[32] (+0x464): the egg slides and the life orbs around moby 10; one set of
  // rects while the display states (+0x00..+0x04) hold.
  static constexpr std::uint32_t kHudRects = 0x464u;
  static constexpr std::size_t kHudRectCount = 32;
  static constexpr std::array<Key, 2> kHudKeys{{{0x00u}, {0x04u, 0xFFu}}};
  static constexpr std::array<Field, kHudRectCount> kHudRectFields = [] {
    std::array<Field, kHudRectCount> fields{};
    for (std::size_t i = 0; i < kHudRectCount; ++i) {
      fields[i] = {kHudRects + static_cast<std::uint32_t>(i) * 8u, psx::present::Motion::Halves};
    }
    return fields;
  }();

  // [g_Particles]: 256 0x20-byte records up to the first whose kind (+1) is 0xFF; a negative kind
  // is free. One particle while its group and kind halfword holds.
  static constexpr std::array<Key, 1> kParticleKeys{{{0x00u, 0xFFFFu}}};
  static constexpr Records kParticleRecords{.throughPointer = true,
                                            .stride = 0x20u,
                                            .count = 0x100u,
                                            .end = RecordByte{0x01u, 0xFFu, 0xFFu},
                                            .live = RecordByte{0x01u, 0x00u, 0x7Fu}};
  static constexpr std::array<Field, 4> kParticle{{
      {0x00u, kStep},
      {0x04u, kStep},
      {0x08u, kStep},
      {0x0Cu, kStep},
  }};

  // g_Sparkles: alive while the fade (+0x0C) is not zero.
  static constexpr Records kSparkleRecords{.stride = glows::kSparkleBytes,
                                           .count = glows::kSparkleCount,
                                           .live = RecordByte{0x0Cu, 0x01u, 0xFFu}};
  static constexpr std::array<Field, 4> kSparkle{{
      {0x00u, kStep},
      {0x04u, kStep},
      {0x08u, kStep},
      {0x0Cu, kStep},
  }};

  // g_SpyroFlame: drawn while m_IsFlameActive is set.
  static constexpr Records kFlameRecords{
      .live = RecordByte{flame_block::kActive - flame_block::kBlock, 0x01u, 0xFFu}};
  static constexpr std::array<Field, 7> kFlameFields{{
      {0x00u, kStep},
      {0x04u, kStep},
      {0x08u, kStep},
      {0x20u, kStep},
      {0x24u, kStep},
      {0x28u, kStep},
      {0x2Cu, kStep},
  }};

  // The section's own counters: the fade it draws (counts down; a raise is a new fade), the
  // border height and portal yaw it ages before reading, the field tick the portal pulse reads and
  // the specular time the life orbs pulse with (& 0xFF).
  static constexpr std::uint32_t kFade = 0x80075918u;
  static constexpr std::uint32_t kBorderSize = 0x800756C0u;
  static constexpr std::uint32_t kPortalYaw = 0x80075858u;
  static constexpr std::array<Field, 1> kCountdown{{{0x00u, psx::present::Motion::Countdown}}};
  static constexpr std::array<Field, 1> kScalar{{{0x00u, kLinear}}};
  static constexpr std::array<Field, 1> kAngle12{{{0x00u, psx::present::Motion::Angle12Low}}};
  static constexpr std::array<Field, 1> kAngle8{{{0x00u, psx::present::Motion::Angle8x3}}};

  SpyroPoses poses_;
  MobyMotion moby_;
  ParticleMotion particle_;
  SparkleMotion sparkle_;
  FlameMotion flame_;
  std::array<psx::present::MotionTrack, 15> tracks_{{
      {.name = "camera", .address = guest::kCamera, .fields = kCamera},
      {.name = "spyro", .address = guest::kSpyro, .fields = kSpyro},
      {.name = "spyro poses", .address = guest::kSpyro, .fields = kSpyroPoses, .rule = &poses_},
      {.name = "spyro shadow", .address = shadow_block::kAnchor, .fields = kVector},
      {.name = "mobys",
       .address = guest::kLevelMobys,
       .fields = kMoby,
       .rule = &moby_,
       .keys = kMobyKeys,
       .records = kMobyRecords},
      {.name = "hud mobys",
       .address = kHudMobys,
       .fields = kHudMoby,
       .keys = kHudMobyKeys,
       .records = kHudMobyRecords},
      {.name = "hud rects", .address = kHud, .fields = kHudRectFields, .keys = kHudKeys},
      {.name = "particles",
       .address = particles::kListBase,
       .fields = kParticle,
       .rule = &particle_,
       .keys = kParticleKeys,
       .records = kParticleRecords},
      {.name = "sparkles",
       .address = glows::kSparkles,
       .fields = kSparkle,
       .rule = &sparkle_,
       .records = kSparkleRecords},
      {.name = "flame",
       .address = flame_block::kBlock,
       .fields = kFlameFields,
       .rule = &flame_,
       .records = kFlameRecords},
      {.name = "fade", .address = kFade, .fields = kCountdown},
      {.name = "border", .address = kBorderSize, .fields = kScalar},
      {.name = "portal yaw", .address = kPortalYaw, .fields = kAngle12},
      {.name = "level ticks", .address = guest::kLevelTicks, .fields = kScalar},
      {.name = "specular time", .address = guest::kSpecularTime, .fields = kAngle8},
  }};
};

} // namespace spyro1
