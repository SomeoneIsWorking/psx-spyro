#include "render/field/moby_motion.h"

#include "render/field/guest_moby_visibility.h"
#include "render/field/record_blend.h"

namespace spyro {
namespace {

namespace record = guest_moby::record;

keyframe_blend::Pose poseOf(const moby_record::Bytes &bytes) {
  return keyframe_blend::Pose{.keyframes =
                                  record_blend::load<std::uint32_t>(bytes, record::kKeyframes),
                              .progress = bytes[record::kKeyProgress]};
}

} // namespace

void MobyRecordMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  moby_record::Bytes from{};
  moby_record::Bytes to{};
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    record_blend::store(from, fields_[i].offset, samples.from[i]);
    record_blend::store(to, fields_[i].offset, samples.to[i]);
  }
  const moby_record::Between moved = moby_record::between(from, to, t);
  moby_record::Bytes shown = moved.bytes;
  finish(samples, from, to, moved, t, shown);
  const std::uint32_t mesh = record_blend::load<std::uint16_t>(to, moby_record::kMesh);
  // A pose that did not step needs no loop, and a model that never animates has no headers to read.
  const std::optional<keyframe_blend::Loop> loop =
      poseOf(from).keyframes == poseOf(to).keyframes ? std::nullopt
                                                     : loopOf(mesh, poseOf(from).keyframes & 0xFFu);
  keyframe_blend::Pose pose = keyframe_blend::between(poseOf(from), poseOf(to), t, progress_, loop);
  if (!keyframe_blend::continues(poseOf(from), poseOf(to), progress_, loop)) {
    const keyframe_blend::Pose crossing =
        keyframe_blend::across(poseOf(from), poseOf(to), t, progress_);
    if (sharesLayout(mesh, crossing)) {
      pose = crossing;
    }
  }
  record_blend::store(shown, record::kKeyframes, pose.keyframes);
  shown[record::kKeyProgress] = pose.progress;
  for (std::size_t i = 0; i < fields_.size(); ++i) {
    out[i] = record_blend::load<std::uint32_t>(shown, fields_[i].offset);
  }
}

} // namespace spyro
