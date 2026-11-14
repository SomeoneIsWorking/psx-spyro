// draw_motion.h — the GS_Playing section's moving inputs as motion tracks
// (docs/plans/spyro1-motion-tracks.md).
#pragma once

#include "motion_set.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_motion.h"
#include "spyro1/core/effect/flame_block.h"
#include "spyro1/core/effect/shadow_block.h"
#include "spyro1/core/frame/dragon_shrink.h"
#include "spyro1/core/frame/exit_fade.h"
#include "spyro1/core/frame/screen_fade.h"
#include "spyro1/core/frame/texture_scroll.h"
#include "spyro1/core/frame/title_slide.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/glows.h"
#include "spyro1/render/particles.h"
#include "spyro1/render/replay_angles.h"
#include "spyro1/render/rescue_star.h"

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
// unit the guest stores; the fine angles are noted for the camera's matrices while any has a
// fraction.
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

// A particle record: its points and, for turned kinds, its angle.
class ParticleMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// A sparkle: position, angle, and its fade while it counts down (0x800584C4). 0x8005882C hands out
// only a slot whose fade is 0, so a fade that rose is a new sparkle.
class SparkleMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The flame burst: its anchor, and each part's length while it grows toward an unchanged limit.
class FlameMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The level-transition tally's gems, in g_anActorShadowQueue (AnimateGemTallyParticles 0x8002DA74
// points g_pGemTallyParticleRecords there each tick): theirs only while LevelTransition_Draw
// 0x8001A050 draws the tally, in GS_LevelTransition or GS_ArrivalCamera.
class TallyLiveness final : public psx::present::MotionLiveness {
public:
  void sample(Core &core) override;
  [[nodiscard]] bool live(std::uint32_t /*record*/) const override {
    return drawn_;
  }

private:
  bool drawn_ = false;
};

class DrawMotion {
public:
  DrawMotion(Core &core, NativeState &state)
      : camera_(state.replayCamera), poses_(core), moby_(core, state.replayAngles),
        particleRecords_{.throughPointer = true,
                         .stride = particles::kRecordBytes,
                         .count = ParticleLives::kSlots,
                         .end = RecordByte{0x01u, 0xFFu, 0xFFu},
                         .live = RecordByte{0x01u, 0x00u, 0x7Fu},
                         .incarnation = &state.particleLives} {}

  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }
  [[nodiscard]] std::span<psx::present::MotionSource *const> sources() const {
    return sources_;
  }
  [[nodiscard]] const ScreenFade &screenFade() const {
    return screenFade_;
  }
  [[nodiscard]] const DragonShrink &dragonShrink() const {
    return dragonShrink_;
  }
  [[nodiscard]] const ExitFade &exitFade() const {
    return exitFade_;
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
  // is free (particleRecords_). One particle while its group and kind halfword holds, within one
  // life the allocator began.
  static constexpr std::array<Key, 1> kParticleKeys{{{0x00u, 0xFFFFu}}};
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

  // The section's own counters: the border height and portal yaw it ages before reading, the field
  // tick the portal pulse reads and the specular time the life orbs pulse with (& 0xFF).
  static constexpr std::uint32_t kBorderSize = 0x800756C0u;
  static constexpr std::uint32_t kPortalYaw = 0x80075858u;
  static constexpr std::array<Field, 1> kScalar{{{0x00u, kLinear}}};
  static constexpr std::array<Field, 1> kAngle12{{{0x00u, psx::present::Motion::Angle12Low}}};
  static constexpr std::array<Field, 1> kAngle8{{{0x00u, psx::present::Motion::Angle8x3}}};

  // The rescue star over a freed dragon, drawn while +0x00 is set: its centre, scale, and the
  // angles 0x8001CFDC turns into its matrix (the colour byte after them steps).
  static constexpr Records kRescueStarRecords{.live = RecordByte{0x00u, 0x01u, 0xFFu}};
  static constexpr std::array<Field, 5> kRescueStar{{
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x0Cu, kLinear},
      {0x24u, kLinear},
      {0x28u, psx::present::Motion::Angle8x3},
  }};

  // LevelTransition_Draw's tally phase (g_nLevelTransitionPhase, up g_DeltaTime a tick), which
  // places its text, counts and rocking, and the sky tilt the draw lowers by 2 before reading.
  static constexpr std::uint32_t kTallyPhase = 0x800756ACu;
  static constexpr std::uint32_t kSkyTilt = 0x80075910u;
  // A tally gem: x and y halfwords, then spin (+0x0C) beside its constant signs and spin rate. One
  // gem while its velocity word (+0x04), drawn at spawn, holds; live while +0x08 is set.
  static constexpr std::uint32_t kTallyGems = 0x800724F4u;
  static constexpr std::array<Key, 1> kTallyGemKeys{{{0x04u}}};
  static constexpr std::array<Field, 2> kTallyGem{{
      {0x00u, psx::present::Motion::Halves},
      {0x0Cu, psx::present::Motion::Angle8x3},
  }};

  CameraMotion camera_;
  SpyroPoses poses_;
  MobyMotion moby_;
  ParticleMotion particle_;
  Records particleRecords_;
  SparkleMotion sparkle_;
  FlameMotion flame_;
  TextureScroll textureScroll_;
  TitleSlide titleSlide_;
  ScreenFade screenFade_;
  DragonShrink dragonShrink_;
  ExitFade exitFade_;
  TallyLiveness tallyLiveness_;
  Records tallyGemRecords_{.stride = 0x10u,
                           .count = 0x20u,
                           .live = RecordByte{0x08u, 0x01u, 0xFFu},
                           .liveness = &tallyLiveness_};
  std::array<psx::present::MotionSource *, 5> sources_{
      &textureScroll_, &titleSlide_, &screenFade_, &dragonShrink_, &exitFade_};
  std::array<psx::present::MotionTrack, 19> tracks_{{
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
       .records = particleRecords_},
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
      {.name = "border", .address = kBorderSize, .fields = kScalar},
      {.name = "portal yaw", .address = kPortalYaw, .fields = kAngle12},
      {.name = "level ticks", .address = guest::kLevelTicks, .fields = kScalar},
      {.name = "specular time", .address = guest::kSpecularTime, .fields = kAngle8},
      {.name = "dragon fade", .address = guest::kDragonCutsceneFade, .fields = kScalar},
      {.name = "rescue star",
       .address = rescue_star::kBlock,
       .fields = kRescueStar,
       .records = kRescueStarRecords},
      {.name = "tally phase", .address = kTallyPhase, .fields = kScalar},
      {.name = "sky tilt", .address = kSkyTilt, .fields = kScalar},
      {.name = "tally gems",
       .address = kTallyGems,
       .fields = kTallyGem,
       .keys = kTallyGemKeys,
       .records = tallyGemRecords_},
  }};
};

} // namespace spyro1
