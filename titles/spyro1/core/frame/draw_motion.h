// draw_motion.h — the GS_Playing section's moving inputs as motion tracks
// (docs/plans/spyro1-motion-tracks.md).
#pragma once

#include "motion_set.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_motion.h"
#include "spyro1/core/effect/flame_block.h"
#include "spyro1/core/effect/shadow_block.h"
#include "spyro1/core/frame/texture_scroll.h"
#include "spyro1/core/frame/title_slide.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/glows.h"
#include "spyro1/render/particles.h"
#include "spyro1/render/replay_angles.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace spyro1 {

// Spyro's body, head and tail poses (g_Spyro +0x18..+0x27): each part's animation pair and progress
// step one unit, blended as keyframe_blend does for the guest's 16-step weight.
class SpyroPoses final : public psx::present::MotionRule {
public:
  explicit SpyroPoses(Core &core) : core_(core) {}
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;

private:
  Core &core_;
};

// g_Camera's m_Position straight and m_Rotation's three angles the short way round, each to the
// unit the guest stores and with its fraction noted for the camera's matrices.
class CameraMotion final : public psx::present::MotionRule {
public:
  explicit CameraMotion(std::optional<spyro::guest_camera::Angles> &noted) : noted_(noted) {}
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;

private:
  std::optional<spyro::guest_camera::Angles> &noted_;
};

// A moby slot, with its shadow word and scale override. Spyro 1's record keeps no sixteenths of an
// angle byte, so the angle bytes are written at the step each lies past and the sixteenths noted
// aside for the list builders.
class MobyMotion final : public spyro::MobyRecordMotion {
public:
  // +0x1C: the shadow word.
  static constexpr auto kFields = spyro::mobyFieldsWith<1>({0x1Cu});

  // A moby's progress byte counts 64 steps (weight k << 6, 0x8001F158).
  MobyMotion(Core &core, actors::ReplayAngles &angles)
      : MobyRecordMotion(kFields, spyro::keyframe_blend::Progress{64}), core_(core),
        angles_(angles) {}

private:
  // The class's mesh (the emitter reads both frames with frame A's counts and shifts).
  [[nodiscard]] bool sharesLayout(std::uint32_t mesh,
                                  const spyro::keyframe_blend::Pose &pose) const override;
  // 0x800522C0 steps a frame past the animation header's count back to 0.
  [[nodiscard]] std::optional<spyro::keyframe_blend::Loop>
  loopOf(std::uint32_t mesh, std::uint32_t animation) const override;
  void finish(const Samples &samples,
              const spyro::moby_record::Bytes &from,
              const spyro::moby_record::Bytes &to,
              const spyro::moby_record::Between &moved,
              float t,
              spyro::moby_record::Bytes &shown) const override;

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
  DrawMotion(Core &core, NativeState &state)
      : camera_(state.replayCamera), poses_(core), moby_(core, state.replayAngles) {}

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }
  [[nodiscard]] std::span<psx::present::MotionSource *const> sources() const {
    return sources_;
  }

private:
  using Field = psx::present::MotionField;
  using Key = psx::present::MotionKey;
  using Records = psx::present::MotionRecords;
  using RecordByte = psx::present::RecordByte;
  static constexpr psx::present::Motion kLinear = psx::present::Motion::Linear;
  static constexpr psx::present::Motion kStep = psx::present::Motion::Step;

  // g_Camera: m_Position, then m_Rotation's three 12-bit halfwords (CameraMotion).
  static constexpr std::array<Field, 5> kCamera{{
      {0x28u, kLinear},
      {0x2Cu, kLinear},
      {0x30u, kLinear},
      {0x4Cu, kStep},
      {0x50u, kStep},
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

  // [g_LevelMobys]: bit 7 of the state set is dead. One object while the props pointer (+0x00),
  // class (+0x36) and screen-space flag (+0x50 bit 7) hold.
  static constexpr std::array<Key, 3> kMobyKeys{{
      {0x00u},
      {0x34u, 0xFFFF0000u},
      {0x50u, 0x80u},
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

  CameraMotion camera_;
  SpyroPoses poses_;
  MobyMotion moby_;
  ParticleMotion particle_;
  SparkleMotion sparkle_;
  FlameMotion flame_;
  TextureScroll textureScroll_;
  TitleSlide titleSlide_;
  std::array<psx::present::MotionSource *, 2> sources_{&textureScroll_, &titleSlide_};
  std::array<psx::present::MotionTrack, 15> tracks_{{
      {.name = "camera", .address = guest::kCamera, .fields = kCamera, .rule = &camera_},
      {.name = "spyro", .address = guest::kSpyro, .fields = kSpyro},
      {.name = "spyro poses", .address = guest::kSpyro, .fields = kSpyroPoses, .rule = &poses_},
      {.name = "spyro shadow", .address = shadow_block::kAnchor, .fields = kVector},
      {.name = "mobys",
       .address = guest::kLevelMobys,
       .fields = MobyMotion::kFields,
       .rule = &moby_,
       .keys = kMobyKeys,
       .records = spyro::kMobyRecords},
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
