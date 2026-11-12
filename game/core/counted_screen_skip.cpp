#include "core/counted_screen_skip.h"

#include "core.h"
#include "core/even_skip.h"

#include <algorithm>
#include <lucent/log.h>
#include <optional>

namespace spyro {
namespace {

// Updates left before a count moving by `step` leaves from `value`, the leaving one included.
template <typename Count> std::uint32_t remainingUpdates(const Count &count, std::int32_t value) {
  const std::int32_t distance = count.step > 0 ? count.limit - value : value - count.limit;
  if (distance < 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(distance / (count.step > 0 ? count.step : -count.step)) + 1u;
}

// Where a press moves the count, keeping `minimum` updates and the buffer parity; none when it
// is already that close.
template <typename Count>
std::optional<std::int32_t>
endedCount(const Count &count, std::int32_t value, std::uint32_t minimum) {
  const std::uint32_t remaining = remainingUpdates(count, value);
  const std::uint32_t kept = keptUpdates(remaining, minimum);
  if (kept >= remaining) {
    return std::nullopt;
  }
  return value + static_cast<std::int32_t>(remaining - kept) * count.step;
}

// Whether any listed word is non-zero; `none` when the list is empty.
bool anySet(Core &core, const std::array<std::uint32_t, 2> &words, bool none) {
  bool listed = false;
  for (const std::uint32_t word : words) {
    if (word == 0u) {
      continue;
    }
    listed = true;
    if (core.mem_r32(word) != 0u) {
      return true;
    }
  }
  return listed ? false : none;
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
        !anySet(core, screen.exceptWhile, false) && anySet(core, screen.onlyWhile, true)) {
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
  const std::optional<std::int32_t> to =
      endedCount(*screen, value, minimumKept(core, *screen, value));
  if (!to) {
    return;
  }
  core.mem_w32(screen->word, static_cast<std::uint32_t>(*to));
  lucent::info("transition", "Start/Cross ends {}: {} -> {}", screen->name, value, *to);
}

void CountedScreenSkip::observeLoop(psx::cpu::ResumableGuestCall &call, bool pressed) {
  const std::uint32_t resumePc = call.resumePc();
  const auto loop =
      std::find_if(facts_.loops.begin(), facts_.loops.end(), [resumePc](const CountedLoop &l) {
        return l.waitReturn == resumePc;
      });
  if (loop == facts_.loops.end() || !pressed) {
    return;
  }
  const auto value = static_cast<std::int32_t>(call.suspendedRegister(loop->reg));
  const std::optional<std::int32_t> to = endedCount(*loop, value, loop->minimumKept);
  if (!to) {
    return;
  }
  call.setSuspendedRegister(loop->reg, static_cast<std::uint32_t>(*to));
  lucent::info("transition", "Start/Cross ends {}: {} -> {}", loop->name, value, *to);
}

} // namespace spyro
