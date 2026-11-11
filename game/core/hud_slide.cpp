#include "core/hud_slide.h"

#include "blend.h"
#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "replay_frame.h"

#include <algorithm>
#include <utility>

namespace spyro {
namespace {

// Every drawer skips a slot whose state is 0.
bool drawn(const HudSlide::Sample &sample) {
  return sample.state != 0;
}

// One call's element: its index offset and its y.
struct Call {
  std::int32_t index = 0;
  std::int32_t y = 0;
};

// Spyro 2's slide function at one sample of the slot.
std::int32_t
offsetOf(Core &core, const HudSlideFacts &facts, const HudSlide::Sample &sample, Call call) {
  std::int32_t shown = facts.restOffset;
  if (sample.state == facts.slidingIn || sample.state == facts.slidingOut) {
    const std::int32_t at = std::clamp(sample.slide + call.index, 0, facts.lastIndex);
    const std::uint32_t table = sample.state == facts.slidingIn ? facts.inTable : facts.outTable;
    shown = core.mem_r8(table + static_cast<std::uint32_t>(at >> 1));
  }
  return call.y > facts.lowerHalfY ? -shown : shown;
}

} // namespace

// f(slot $a0, offset $a1, y $a2) -> $v0; it leaves the result in $v1 too (0x80052514).
void slideFunction(Core *core, const HudSlideFacts &facts) {
  HudSlide &slide = core->game->frameDriverAs<BootPrefixFrameDriver>().motion().hudSlide();
  const std::uint32_t slot = core->r[4];
  const Call call{.index = static_cast<std::int32_t>(core->r[5]),
                  .y = static_cast<std::int32_t>(core->r[6])};
  std::int32_t shown = offsetOf(*core, facts, slide.now(*core, slot), call);
  slide.settle();
  const std::optional<float> t = core->runtime.replayFrame(*core)->replaying();
  if (const std::optional<HudSlide::Sample> earlier =
          t ? slide.earlier(*core, slot) : std::nullopt) {
    const std::int32_t from = offsetOf(*core, facts, *earlier, call);
    shown = slide.between(*core, {.y = from}, {.y = shown}, *t).y;
  }
  core->r[2] = static_cast<std::uint32_t>(shown);
  core->r[3] = static_cast<std::uint32_t>(shown);
}

std::optional<std::size_t> HudSlide::slotIndex(std::uint32_t slot) const {
  if (slot < slots_->firstSlot || (slot - slots_->firstSlot) % slots_->slotBytes != 0) {
    return std::nullopt;
  }
  const std::size_t index = (slot - slots_->firstSlot) / slots_->slotBytes;
  return index < slots_->slotCount ? std::optional(index) : std::nullopt;
}

HudSlide::Sample HudSlide::read(Core &core, std::uint32_t slot) const {
  const std::uint32_t slide = slot + slots_->slideOffset;
  return {.state = core.mem_r8(slot + slots_->stateOffset),
          .slide = slots_->slideIsByte ? core.mem_r8(slide)
                                       : static_cast<std::int16_t>(core.mem_r16(slide))};
}

void HudSlide::sample(Core &core) {
  if (slots_ == nullptr) {
    return;
  }
  from_ = to_;
  for (std::size_t i = 0; i < slots_->slotCount; ++i) {
    to_[i] = read(core, slots_->firstSlot + static_cast<std::uint32_t>(i) * slots_->slotBytes);
  }
}

psx::present::MotionCount HudSlide::write(psx::present::GuestRedraw &, float) const {
  if (slots_ == nullptr) {
    return {};
  }
  psx::present::MotionCount count;
  for (std::size_t i = 0; i < slots_->slotCount; ++i) {
    if (drawn(to_[i])) {
      ++count.drawn;
      count.unpaired += drawn(from_[i]) ? 0u : 1u;
    }
  }
  return count;
}

HudSlide::Sample HudSlide::now(Core &core, std::uint32_t slot) const {
  return read(core, slot);
}

std::optional<HudSlide::Sample> HudSlide::earlier(Core &core, std::uint32_t slot) const {
  const std::optional<std::size_t> sampled = slots_ != nullptr ? slotIndex(slot) : std::nullopt;
  if (!sampled || !drawn(from_[*sampled]) || !drawn(read(core, slot))) {
    return std::nullopt;
  }
  return from_[*sampled];
}

HudSlide::Place HudSlide::between(const Core &core, Place from, Place to, float t) {
  const Place shown{.x = psx::present::lerpInt(from.x, to.x, t),
                    .y = psx::present::lerpInt(from.y, to.y, t)};
  kept_ = {.place = {.x = static_cast<float>(psx::present::lerpExact(from.x, to.x, t) - shown.x),
                     .y = static_cast<float>(psx::present::lerpExact(from.y, to.y, t) - shown.y)},
           .run = core.writeRollback.run(),
           .sp = core.r[psx::cpu::kSp]};
  return shown;
}

void HudSlide::settle() {
  kept_ = {};
}

HudSlide::Remainder HudSlide::takeRemainder(const Core &core) {
  const Kept taken = std::exchange(kept_, {});
  const bool same = core.writeRollback.armed() && taken.run == core.writeRollback.run() &&
                    taken.sp == core.r[psx::cpu::kSp];
  return same ? taken.place : Remainder{};
}

} // namespace spyro
