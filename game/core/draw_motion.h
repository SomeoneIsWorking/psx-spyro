// draw_motion.h — the Spyro 2/3 draw body's moving inputs as motion tracks
// (docs/plans/spyro23-replay.md).
#pragma once

#include "core/hud_slide.h"
#include "core/sky_wave.h"
#include "core/table_slide.h"
#include "motion_set.h"
#include "render/field/effect_pools.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/moby_motion.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace spyro {

// A conversation box's slide table: in `mode`, the box is placed at entry (timer - firstTick) / 2.
struct SlideTable {
  std::uint32_t mode = 0;
  std::uint32_t table = 0; // signed halfwords
  std::uint32_t firstTick = 0;
  std::uint32_t entries = 0;
};

// The conversation state's box slide: its mode and timer words and the tables it slides by.
struct ConversationFacts {
  std::uint32_t gameState = 0;
  std::uint32_t state = 0;
  std::uint32_t mode = 0;
  std::uint32_t timer = 0;
  std::array<SlideTable, 2> tables{};
};

inline constexpr std::size_t kMaxFades = 3;

struct FadeWord {
  const char *name = "";
  std::uint32_t address = 0;
  // A fade that ramps both ways is Linear; one set at once and lowered by the draw is a Countdown.
  psx::present::MotionField field{0x00u, psx::present::Motion::Linear};
};

struct DrawMotionFacts {
  std::uint32_t cameraAngles = 0;   // three 12-bit angle halfwords
  std::uint32_t cameraPosition = 0; // x, y, z words
  // Spyro's record: position words, body and head angle bytes (+0xC, +0x10), body and head
  // keyframes (+0x14, +0x18) and their nibble progress bytes (+0x1C, +0x1D).
  std::uint32_t spyro = 0;
  // His shadow: position words, then three angle bytes and a flag byte.
  std::uint32_t shadow = 0;
  // Spyro's flame breath: the anchor words the update places at his mouth, then +0x98 the byte
  // it is drawn while, +0x99 the parts it has lit, +0x9A its aim and +0x9B its path row.
  std::uint32_t flame = 0;
  const guest_moby::Facts *moby = nullptr;
  effect_pools::Facts effects;
  // The fade words the dispatcher draws while non-zero, `fadeCount` of them.
  std::array<FadeWord, kMaxFades> fades{};
  std::size_t fadeCount = 0;
  const ConversationFacts *conversation = nullptr;
  const HudSlideFacts *hudSlide = nullptr;
  const SkyWaveFacts *skyWave = nullptr;
};

// Spyro's tint (+0x20), water floor (+0x28), reflection shift (+0x2C) and the body rotation the
// water pass reads (+0x30): each blends only while both samples have it on.
class SpyroSurface final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The flame's anchor while one breath lasts: a breath begins with its lit count at 0, so a count
// that fell is a new breath, drawn as it is now.
class FlameMotion final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// The conversation box's slide entry.
class ConversationSlide final : public TableSlide {
public:
  explicit ConversationSlide(const ConversationFacts *facts) : facts_(facts) {}
  [[nodiscard]] const char *name() const override {
    return "conversation slide";
  }

private:
  [[nodiscard]] std::optional<Entry> entryOf(Core &core) const override;

  const ConversationFacts *facts_;
};

// Spyro's body and head poses: each part's keyframe pair and nibble progress.
class SpyroPoses final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// A moby slot. Spyro 2/3's walk reads the sixteenths an angle byte drops from the flag word's
// nibbles (bit 30 set), so an in-between refines them there when both samples carry them.
class MobyMotion final : public MobyRecordMotion {
public:
  static constexpr auto kFields = mobyFieldsWith<0>({});

  MobyMotion(Core &core, const guest_moby::Facts &walk)
      : MobyRecordMotion(kFields, keyframe_blend::kNibbleSteps), core_(core), walk_(walk) {}

private:
  [[nodiscard]] bool sharesLayout(std::uint32_t mesh,
                                  const keyframe_blend::Pose &pose) const override;
  [[nodiscard]] std::optional<keyframe_blend::Loop> loopOf(std::uint32_t mesh,
                                                           std::uint32_t animation) const override;
  void finish(const Samples &samples,
              const moby_record::Bytes &from,
              const moby_record::Bytes &to,
              const moby_record::Between &moved,
              float t,
              moby_record::Bytes &shown) const override;

  Core &core_;
  const guest_moby::Facts &walk_;
};

// The effects pass's pools, paired by effect_pools' own rules.
class EffectMotion final : public psx::present::MotionSource {
public:
  explicit EffectMotion(const effect_pools::Facts &facts) : facts_(facts) {}
  [[nodiscard]] const char *name() const override {
    return "effects";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

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
  [[nodiscard]] HudSlide &hudSlide() {
    return hudSlide_;
  }
  [[nodiscard]] const SkyWave &skyWave() const {
    return skyWave_;
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
  // SpyroSurface's words, in its order.
  static constexpr std::array<Field, 8> kSpyroSurface{{
      {0x20u, kStep},
      {0x28u, kStep},
      {0x2Cu, kStep},
      {0x30u, kStep},
      {0x34u, kStep},
      {0x38u, kStep},
      {0x3Cu, kStep},
      {0x40u, kStep},
  }};
  static constexpr psx::present::MotionRecords kFlameRecords{
      .live = psx::present::RecordByte{0x98u, 0x01u, 0xFFu}};
  // FlameMotion's words: the anchor, then the word of its state bytes.
  static constexpr std::array<Field, 4> kFlame{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x98u, kStep},
  }};
  // One breath while its aim and path row hold.
  static constexpr std::array<psx::present::MotionKey, 1> kFlameKeys{{{0x98u, 0xFFFF0000u}}};
  // Position words, then three angle bytes whose top byte is the hidden flag.
  static constexpr std::array<Field, 4> kShadow{{
      {0x00u, kLinear},
      {0x04u, kLinear},
      {0x08u, kLinear},
      {0x0Cu, kAngle8},
  }};
  // One moby while the mesh (+0x36) holds.
  static constexpr std::array<psx::present::MotionKey, 1> kMobyKeys{{{0x34u, 0xFFFF0000u}}};

  SpyroPoses poses_;
  SpyroSurface surface_;
  FlameMotion flame_;
  MobyMotion moby_;
  EffectMotion effects_;
  ConversationSlide conversation_;
  HudSlide hudSlide_;
  SkyWave skyWave_;
  std::vector<psx::present::MotionTrack> tracks_;
  std::array<psx::present::MotionSource *, 4> sources_{
      &effects_, &conversation_, &hudSlide_, &skyWave_};
};

} // namespace spyro
