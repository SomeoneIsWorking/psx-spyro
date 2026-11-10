#include "core/draw_motion.h"

#include "blend.h"
#include "core.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_record.h"
#include "render/field/record_blend.h"

namespace spyro {
namespace {

namespace record = guest_moby::record;

std::uint32_t flagsOf(const moby_record::Bytes &bytes) {
  return record_blend::load<std::uint32_t>(bytes, record::kFlags);
}

} // namespace

void SpyroPoses::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  // out: body keyframes, head keyframes, the progress word (body byte, head byte, then two that
  // step).
  std::uint32_t progress = samples.to[2];
  for (std::uint32_t part = 0; part < 2; ++part) {
    const std::uint32_t shift = 8 * part;
    const auto pose = [&](std::span<const std::uint32_t> words) {
      return keyframe_blend::Pose{.keyframes = words[part],
                                  .progress = static_cast<std::uint8_t>(words[2] >> shift)};
    };
    const keyframe_blend::Pose shown =
        keyframe_blend::between(pose(samples.from), pose(samples.to), t);
    out[part] = shown.keyframes;
    progress = (progress & ~(0xFFu << shift)) | (std::uint32_t{shown.progress} << shift);
  }
  out[2] = progress;
}

bool MobyMotion::sharesLayout(std::uint32_t mesh, const keyframe_blend::Pose &pose) const {
  return guest_moby::slotsShareLayout(core_, walk_, mesh, pose.keyframes);
}

void MobyMotion::finish(const Samples &,
                        const moby_record::Bytes &from,
                        const moby_record::Bytes &to,
                        const moby_record::Between &moved,
                        float t,
                        moby_record::Bytes &shown) const {
  if ((flagsOf(from) & flagsOf(to) & record::kInterpolatedAngles) == 0) {
    shown = moby_record::nearest(moved);
    return;
  }
  std::uint32_t flags = flagsOf(to);
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t shift = axis * 8;
    const auto refined = [&](const moby_record::Bytes &bytes) {
      return (std::uint32_t{bytes[moby_record::kAngles + axis]} << 4) |
             ((flagsOf(bytes) >> shift) & 0xFu);
    };
    const std::uint32_t angle = psx::present::lerpTurn(refined(from), refined(to), 12, t);
    shown[moby_record::kAngles + axis] = static_cast<std::uint8_t>(angle >> 4);
    flags = (flags & ~(0xFu << shift)) | ((angle & 0xFu) << shift);
  }
  record_blend::store(shown, record::kFlags, flags);
}

void EffectMotion::sample(Core &core) {
  later_ ^= 1u;
  effect_pools::read(core, facts_, records_[later_]);
}

void EffectMotion::write(psx::present::GuestRedraw &redraw, float t) const {
  effect_pools::writeBetween(redraw, facts_, records_[later_ ^ 1u], records_[later_], t);
}

DrawMotion::DrawMotion(Core &core, const DrawMotionFacts &facts)
    : moby_(core, *facts.moby), effects_(facts.effects),
      tracks_{{
          {.name = "camera angles", .address = facts.cameraAngles, .fields = kCameraAngles},
          {.name = "camera position", .address = facts.cameraPosition, .fields = kVector},
          {.name = "spyro", .address = facts.spyro, .fields = kSpyro},
          {.name = "spyro poses", .address = facts.spyro, .fields = kSpyroPoses, .rule = &poses_},
          {.name = "spyro shadow", .address = facts.shadow, .fields = kShadow},
          {.name = "mobys",
           .address = facts.moby->firstRecordWord,
           .fields = MobyMotion::kFields,
           .rule = &moby_,
           .keys = kMobyKeys,
           .records = kMobyRecords},
      }} {}

} // namespace spyro
