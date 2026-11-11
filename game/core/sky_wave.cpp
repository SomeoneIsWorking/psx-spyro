#include "core/sky_wave.h"

#include "blend.h"
#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "replay_frame.h"

#include <cmath>
#include <lucent/log.h>

namespace spyro {
namespace {

void consumer(Core *core) {
  const SkyWave &wave = core->game->frameDriverAs<BootPrefixFrameDriver>().motion().skyWave();
  const SkyWaveFacts &facts = *wave.facts();
  if (const std::optional<float> t = core->runtime.replayFrame(*core)->replaying()) {
    const std::uint32_t object = core->mem_r32(core->r[psx::cpu::kSp] + facts.objectSlot);
    if (const std::optional<std::uint16_t> value =
            wave.between(*core, {.object = object, .t = *t})) {
      lucent::debug("motion",
                    "sky wave object {} at t {}: stored 0x{:04X}, drawn 0x{:04X}",
                    object,
                    *t,
                    core->mem_r16(facts.wave),
                    *value);
      core->mem_w16(facts.wave, *value);
      core->mem_w16(facts.wave + 2, *value);
    }
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, facts.consumer);
}

} // namespace

void SkyWave::sample(Core &core) {
  if (facts_ == nullptr) {
    return;
  }
  from_ = to_;
  to_ = core.mem_r32(facts_->tick);
}

psx::present::MotionCount SkyWave::write(psx::present::GuestRedraw &, float) const {
  if (facts_ == nullptr) {
    return {};
  }
  return {.drawn = 1, .unpaired = from_ ? 0u : 1u};
}

std::optional<std::uint16_t> SkyWave::between(Core &core, SkyWaveDraw draw) const {
  if (facts_ == nullptr || !from_ || !to_) {
    return std::nullopt;
  }
  // The table index wraps at 256 entries, so the earlier entry is taken there and the ticks' signed
  // difference blended from it.
  const auto start =
      static_cast<double>((*from_ * facts_->step + draw.object * facts_->objectStep) & 0xFFu);
  const double steps = static_cast<double>(static_cast<std::int32_t>(*to_ - *from_)) * facts_->step;
  const double entry = psx::present::lerpExact(start, start + steps, draw.t);
  const double below = std::floor(entry);
  const auto first = static_cast<std::uint32_t>(static_cast<std::int64_t>(below));
  const auto sineAt = [&](std::uint32_t index) {
    return static_cast<double>(
        static_cast<std::int16_t>(core.mem_r16(facts_->sine + (index & 0xFFu) * 2u)));
  };
  const double sine =
      psx::present::lerpExact(sineAt(first), sineAt(first + 1u), static_cast<float>(entry - below));
  // 0x8004CBFC..0x8004CC04: the entry's arithmetic shift by four, then the base.
  return static_cast<std::uint16_t>((static_cast<std::int32_t>(std::lround(sine)) >> 4) +
                                    facts_->base);
}

void installSkyWave(Core &core, const SkyWaveFacts &facts) {
  psx::cpu::installNativeOverride(core, facts.consumer, "spyro-sky-wave", consumer);
}

} // namespace spyro
