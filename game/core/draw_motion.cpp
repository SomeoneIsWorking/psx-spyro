#include "core/draw_motion.h"

#include "blend.h"
#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "gte_control.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "render/field/keyframe_blend.h"
#include "render/field/moby_record.h"
#include "render/field/record_blend.h"
#include "replay_frame.h"

#include <algorithm>

namespace spyro {
namespace {

namespace record = guest_moby::record;

std::uint32_t flagsOf(const moby_record::Bytes &bytes) {
  return record_blend::load<std::uint32_t>(bytes, record::kFlags);
}

void allocateParticle(Core *core) {
  EffectMotion &effects = core->game->frameDriverAs<BootPrefixFrameDriver>().motion().effects();
  if (!psx::cpu::callOriginalOrPropagate(*core, effects.facts().allocator) ||
      core->runtime.replayFrame(*core)->replaying()) {
    return;
  }
  effects.spawned(*core, core->r[psx::cpu::kV0]);
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

void SpyroSurface::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  // out: tint, water floor, reflection shift, then the five rotation words.
  const std::span<const std::uint32_t> from = samples.from;
  const std::span<const std::uint32_t> to = samples.to;
  std::copy(to.begin(), to.end(), out.begin());
  // The tint's amount is its top byte (0 = off); the colour steps.
  const std::uint32_t amountFrom = from[0] >> 24;
  const std::uint32_t amountTo = to[0] >> 24;
  if (amountFrom != 0 && amountTo != 0) {
    const auto amount = static_cast<std::uint32_t>(psx::present::lerpInt(
        static_cast<std::int32_t>(amountFrom), static_cast<std::int32_t>(amountTo), t));
    out[0] = (to[0] & 0x00FFFFFFu) | (amount << 24);
  }
  const auto floorFrom = static_cast<std::int32_t>(from[1]);
  const auto floorTo = static_cast<std::int32_t>(to[1]);
  // 0 is no water; a negative floor takes the fog branch of 0x80041DBC.
  if (floorFrom != 0 && floorTo != 0 && (floorFrom < 0) == (floorTo < 0)) {
    out[1] = psx::present::lerpWord(from[1], to[1], t);
  }
  if (from[2] != 0 && to[2] != 0) {
    out[2] = psx::present::lerpWord(from[2], to[2], t);
  }
  if (floorFrom != 0 && floorTo != 0) {
    psx::gte::RotationWords before{};
    psx::gte::RotationWords after{};
    std::copy_n(from.begin() + 3, before.size(), before.begin());
    std::copy_n(to.begin() + 3, after.size(), after.begin());
    const psx::gte::RotationWords shown = psx::present::rotationBetween(before, after, t);
    std::copy(shown.begin(), shown.end(), out.begin() + 3);
  }
}

std::optional<TableSlide::Entry> ConversationSlide::entryOf(Core &core) const {
  if (facts_ == nullptr || core.mem_r32(facts_->gameState) != facts_->state) {
    return std::nullopt;
  }
  const std::uint32_t mode = core.mem_r32(facts_->mode);
  const auto timer = static_cast<std::int32_t>(core.mem_r32(facts_->timer));
  for (const SlideTable &slide : facts_->tables) {
    if (slide.table == 0 || slide.mode != mode ||
        timer < static_cast<std::int32_t>(slide.firstTick)) {
      continue;
    }
    const std::uint32_t index = (static_cast<std::uint32_t>(timer) - slide.firstTick) >> 1;
    if (index >= slide.entries) {
      return std::nullopt;
    }
    return Entry{.phase = mode, .table = slide.table, .index = index, .width = Width::SignedHalf};
  }
  return std::nullopt;
}

bool MobyMotion::sharesLayout(std::uint32_t mesh, const keyframe_blend::Pose &pose) const {
  return guest_moby::slotsShareLayout(core_, walk_, mesh, pose.keyframes);
}

std::optional<keyframe_blend::Loop> MobyMotion::loopOf(std::uint32_t mesh,
                                                       std::uint32_t animation) const {
  return guest_moby::animationLoop(core_, walk_, mesh, animation);
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

void FlameMotion::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  std::copy(samples.to.begin(), samples.to.end(), out.begin());
  const auto lit = [](std::uint32_t state) {
    return (state >> 8) & 0xFFu;
  };
  if (lit(samples.to[3]) < lit(samples.from[3])) {
    return;
  }
  for (std::size_t word = 0; word < 3; ++word) {
    out[word] = psx::present::lerpWord(samples.from[word], samples.to[word], t);
  }
}

void EffectMotion::sample(Core &core) {
  later_ ^= 1u;
  effect_pools::read(core, facts_, records_[later_]);
  records_[later_].spawned = spawned_;
  spawned_.reset();
}

void EffectMotion::spawned(Core &core, std::uint32_t address) {
  spawned_.set((address - core.mem_r32(facts_.particles)) / effect_pools::kParticleBytes);
}

psx::present::MotionCount EffectMotion::write(psx::present::GuestRedraw &redraw, float t) const {
  return effect_pools::writeBetween(redraw, facts_, records_[later_ ^ 1u], records_[later_], t);
}

DrawMotion::DrawMotion(Core &core, const DrawMotionFacts &facts)
    : moby_(core, *facts.moby), effects_(facts.effects), conversation_(facts.conversation),
      hudSlide_(facts.hudSlide), skyWave_(facts.skyWave),
      tracks_{{
          {.name = "camera angles", .address = facts.cameraAngles, .fields = kCameraAngles},
          {.name = "camera position", .address = facts.cameraPosition, .fields = kVector},
          {.name = "spyro", .address = facts.spyro, .fields = kSpyro},
          {.name = "spyro poses", .address = facts.spyro, .fields = kSpyroPoses, .rule = &poses_},
          {.name = "spyro surface",
           .address = facts.spyro,
           .fields = kSpyroSurface,
           .rule = &surface_},
          {.name = "spyro shadow", .address = facts.shadow, .fields = kShadow},
          {.name = "flame",
           .address = facts.flame,
           .fields = kFlame,
           .rule = &flame_,
           .keys = kFlameKeys,
           .records = kFlameRecords},
          {.name = "mobys",
           .address = facts.moby->firstRecordWord,
           .fields = MobyMotion::kFields,
           .rule = &moby_,
           .keys = kMobyKeys,
           .records = kMobyRecords},
      }} {
  for (std::size_t i = 0; i < facts.fadeCount; ++i) {
    tracks_.push_back({.name = facts.fades[i].name,
                       .address = facts.fades[i].address,
                       .fields = std::span(&facts.fades[i].field, 1)});
  }
}

void installEffectSpawns(Core &core, const effect_pools::Facts &facts) {
  psx::cpu::installNativeOverride(core, facts.allocator, "spyro-particle-spawn", allocateParticle);
}

} // namespace spyro
