#include "core/hud_slide.h"

#include "blend.h"
#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "native_dispatch.h"
#include "replay_frame.h"

#include <algorithm>

namespace spyro {
namespace {

std::uint8_t stateOf(std::uint32_t slotWord) {
  return static_cast<std::uint8_t>(slotWord);
}

// Every drawer skips a slot whose state is 0.
bool drawn(std::uint32_t slotWord) {
  return stateOf(slotWord) != 0;
}

// f(slot $a0, offset $a1, y $a2) -> $v0; it leaves the result in $v1 too (0x80052514).
void slideFunction(Core *core) {
  BootPrefixFrameDriver &driver = core->game->frameDriverAs<BootPrefixFrameDriver>();
  const HudSlide::Call call{.slot = core->r[4],
                            .index = static_cast<std::int32_t>(core->r[5]),
                            .y = static_cast<std::int32_t>(core->r[6])};
  const auto shown = static_cast<std::uint32_t>(driver.motion().hudSlide().offset(
      *core, call, core->runtime.replayFrame(*core)->replaying()));
  core->r[2] = shown;
  core->r[3] = shown;
}

} // namespace

std::optional<std::size_t> HudSlide::slotIndex(std::uint32_t slot) const {
  if (slot < facts_->firstSlot || (slot - facts_->firstSlot) % facts_->slotBytes != 0) {
    return std::nullopt;
  }
  const std::size_t index = (slot - facts_->firstSlot) / facts_->slotBytes;
  return index < facts_->slotCount ? std::optional(index) : std::nullopt;
}

void HudSlide::sample(Core &core) {
  if (facts_ == nullptr) {
    return;
  }
  from_ = to_;
  for (std::size_t i = 0; i < facts_->slotCount; ++i) {
    to_[i] = core.mem_r32(facts_->firstSlot + static_cast<std::uint32_t>(i) * facts_->slotBytes);
  }
}

psx::present::MotionCount HudSlide::write(psx::present::GuestRedraw &, float) const {
  if (facts_ == nullptr) {
    return {};
  }
  psx::present::MotionCount count;
  for (std::size_t i = 0; i < facts_->slotCount; ++i) {
    if (drawn(to_[i])) {
      ++count.drawn;
      count.unpaired += drawn(from_[i]) ? 0u : 1u;
    }
  }
  return count;
}

std::int32_t HudSlide::offsetOf(Core &core, std::uint32_t slotWord, const Call &call) const {
  const std::uint8_t state = stateOf(slotWord);
  std::int32_t shown = facts_->restOffset;
  if (state == facts_->slidingIn || state == facts_->slidingOut) {
    const auto slide = static_cast<std::int16_t>(slotWord >> 16);
    const std::int32_t at = std::clamp(slide + call.index, 0, facts_->lastIndex);
    const std::uint32_t table = state == facts_->slidingIn ? facts_->inTable : facts_->outTable;
    shown = core.mem_r8(table + static_cast<std::uint32_t>(at >> 1));
  }
  return call.y > facts_->lowerHalfY ? -shown : shown;
}

std::int32_t HudSlide::offset(Core &core, const Call &call, std::optional<float> t) const {
  const std::uint32_t slotWord = core.mem_r32(call.slot);
  const std::int32_t now = offsetOf(core, slotWord, call);
  const std::optional<std::size_t> sampled = slotIndex(call.slot);
  if (!t || !sampled || !drawn(from_[*sampled]) || !drawn(slotWord)) {
    return now;
  }
  return psx::present::lerpInt(offsetOf(core, from_[*sampled], call), now, *t);
}

void installHudSlide(Core &core, const HudSlideFacts &facts) {
  psx::cpu::installNativeOverride(core, facts.function, "spyro-hud-slide", slideFunction);
}

} // namespace spyro
