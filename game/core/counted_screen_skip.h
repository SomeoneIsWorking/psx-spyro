#pragma once

#include "core/press_latch.h"
#include "resumable_guest_call.h"

#include <array>
#include <cstdint>
#include <span>

class Core;

namespace spyro {

// A screen the player does not control that its update ends on its own count: each update moves a
// word by `step` and leaves once that move passes `limit` (up: past `limit`, down: below it).
struct CountedScreen {
  const char *name = "";
  std::uint32_t state = 0;
  std::uint32_t subState = 0;
  std::uint32_t word = 0;
  std::int32_t step = 0;
  std::int32_t limit = 0;
  // Updates a press still lets run: the last ones the screen draws, one per display buffer.
  std::uint32_t minimumKept = 2;
  // A word still at 0 means the screen has not begun (its first update loads it).
  bool zeroIsBefore = false;
  // While any of these words is non-zero the screen runs another course and is not offered. 0:
  // none.
  std::array<std::uint32_t, 2> exceptWhile{};
  // The screen is offered only while one of these words is non-zero. All 0: always.
  std::array<std::uint32_t, 2> onlyWhile{};
  // An update reaching `mustReach` does work beyond the screen when the byte at
  // `mustReachFlags + [mustReachKind]` is set; a press keeps that update. 0: none.
  std::int32_t mustReach = 0;
  std::uint32_t mustReachFlags = 0;
  std::uint32_t mustReachKind = 0;
  // A press waits until the count reaches this, the end of the screen's fade-in. 0: none.
  std::int32_t offeredFrom = 0;
  // While the word `loadingKind` is below `loadingKinds` the screen only covers a load: the update
  // after the one that began it leaves, its count put where the screen's own test ends it. 0: none.
  std::uint32_t loadingKind = 0;
  std::uint32_t loadingKinds = 0;
};

// A fade that loops inside one guest call, one field per pass: its count is a register of the call
// suspended at the pass's display wait, already moved for the pass on screen; the loop leaves once
// the next move passes `limit`.
struct CountedLoop {
  const char *name = "";
  // The return address of the pass's VSync(0).
  std::uint32_t waitReturn = 0;
  unsigned reg = 0;
  std::int32_t step = 0;
  std::int32_t limit = 0;
  std::uint32_t minimumKept = 2;
  // As CountedScreen's: while [loadingKind] < loadingKinds the loop ends without a press.
  std::uint32_t loadingKind = 0;
  std::uint32_t loadingKinds = 0;
};

struct CountedScreenFacts {
  std::uint32_t gameState = 0; // 0: none
  std::uint32_t subState = 0;
  std::span<const CountedScreen> screens;
  std::span<const CountedLoop> loops;
};

// A Start/Cross press on one of these screens moves its word to where the guest's own test ends it
// within a few updates, so the screen leaves through its own exit at normal speed.
class CountedScreenSkip {
public:
  explicit CountedScreenSkip(const CountedScreenFacts &facts) : facts_(facts) {}

  // Before each frame update.
  void observe(Core &core, bool pressed);
  // At each field of a fade loop, with the call suspended at its wait.
  void observeLoop(Core &core, psx::cpu::ResumableGuestCall &call, bool pressed);

private:
  [[nodiscard]] const CountedScreen *showing(Core &core) const;

  CountedScreenFacts facts_;
  PressLatch press_;
  const CountedScreen *latched_ = nullptr;
};

} // namespace spyro
