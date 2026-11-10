// moby_motion.h — a moby slot between two samples, as all three replays move it.
#pragma once

#include "motion_set.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_record.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace spyro {

// [moby array]: 0x58-byte slots up to the first whose flag (+0x48) is 0xFF; a negative flag skips.
inline constexpr psx::present::MotionRecords kMobyRecords{
    .throughPointer = true,
    .stride = moby_record::kBytes,
    .count = 0x800u,
    .end = psx::present::RecordByte{0x48u, 0xFFu, 0xFFu},
    .live = psx::present::RecordByte{0x48u, 0x00u, 0x7Fu}};

// The words every moby track samples: position, keyframes, progress, angle bytes, +0x54 (Spyro
// 2/3's flag word, Spyro 1's scale override byte) and the word holding the mesh halfword.
inline constexpr std::array<std::uint32_t, 8> kMobyWords{
    0x0Cu, 0x10u, 0x14u, 0x3Cu, 0x40u, 0x44u, 0x54u, 0x34u};

// kMobyWords followed by a title's own words, each stepped by MobyRecordMotion.
template <std::size_t N>
[[nodiscard]] constexpr std::array<psx::present::MotionField, kMobyWords.size() + N>
mobyFieldsWith(const std::array<std::uint32_t, N> &own) {
  std::array<psx::present::MotionField, kMobyWords.size() + N> fields{};
  for (std::size_t i = 0; i < kMobyWords.size(); ++i) {
    fields[i] = {kMobyWords[i], psx::present::Motion::Step};
  }
  for (std::size_t i = 0; i < N; ++i) {
    fields[kMobyWords.size() + i] = {own[i], psx::present::Motion::Step};
  }
  return fields;
}

class MobyRecordMotion : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const final;

protected:
  MobyRecordMotion(std::span<const psx::present::MotionField> fields,
                   keyframe_blend::Progress progress)
      : fields_(fields), progress_(progress) {}

  // Whether `pose`'s two frames of mesh `mesh` share a vertex layout the drawer can blend across.
  [[nodiscard]] virtual bool sharesLayout(std::uint32_t mesh,
                                          const keyframe_blend::Pose &pose) const = 0;
  // The title's angle bytes and own words on `shown`, which starts as `moved`; the pose is written
  // after.
  virtual void finish(const Samples &samples,
                      const moby_record::Bytes &from,
                      const moby_record::Bytes &to,
                      const moby_record::Between &moved,
                      float t,
                      moby_record::Bytes &shown) const = 0;

private:
  std::span<const psx::present::MotionField> fields_;
  keyframe_blend::Progress progress_;
};

} // namespace spyro
