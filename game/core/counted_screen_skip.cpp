#include "core/counted_screen_skip.h"

#include "core.h"
#include "core/even_skip.h"

#include <lucent/log.h>

namespace spyro {
namespace {

// Updates left before the screen leaves from `value`, the leaving one included.
std::uint32_t remainingUpdates(const CountedScreen &screen, std::int32_t value) {
  const std::int32_t distance = screen.step > 0 ? screen.limit - value : value - screen.limit;
  if (distance < 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(distance / (screen.step > 0 ? screen.step : -screen.step)) + 1u;
}

std::uint32_t minimumKept(Core &core, const CountedScreen &screen, std::int32_t value) {
  if (screen.mustReach == 0 || value >= screen.mustReach ||
      core.mem_r8(screen.mustReachFlags + core.mem_r32(screen.mustReachKind)) == 0u) {
    return screen.minimumKept;
  }
  return remainingUpdates(screen, screen.mustReach - screen.step);
}

} // namespace

const CountedScreen *CountedScreenSkip::showing(Core &core) const {
  if (facts_.gameState == 0u) {
    return nullptr;
  }
  const std::uint32_t state = core.mem_r32(facts_.gameState);
  const std::uint32_t subState = core.mem_r32(facts_.subState);
  for (const CountedScreen &screen : facts_.screens) {
    if (screen.state == state && screen.subState == subState &&
        !(screen.zeroIsBefore && core.mem_r32(screen.word) == 0u) &&
        !(screen.exceptWhile != 0u && core.mem_r32(screen.exceptWhile) != 0u)) {
      return &screen;
    }
  }
  return nullptr;
}

void CountedScreenSkip::observe(Core &core, bool pressed) {
  const CountedScreen *screen = showing(core);
  if (screen != latched_) {
    press_.clear();
    latched_ = screen;
  }
  if (screen == nullptr) {
    return;
  }
  press_.observe(pressed);
  if (!press_.take()) {
    return;
  }
  const auto value = static_cast<std::int32_t>(core.mem_r32(screen->word));
  const std::uint32_t remaining = remainingUpdates(*screen, value);
  const std::uint32_t kept = keptUpdates(remaining, minimumKept(core, *screen, value));
  if (kept >= remaining) {
    return;
  }
  const std::int32_t to = value + static_cast<std::int32_t>(remaining - kept) * screen->step;
  core.mem_w32(screen->word, static_cast<std::uint32_t>(to));
  lucent::info("transition", "Start/Cross ends {}: {} -> {}", screen->name, value, to);
}

} // namespace spyro
