#pragma once

#include "core/press_latch.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <span>

class Core;

namespace spyro {

// One boot card's minimum-time hold, `while (VSync(-1) - start < fields) VSync(0)`.
struct LogoHold {
  // The return address of the hold's VSync(0).
  std::uint32_t waitReturn = 0;
  std::uint32_t fields = 0;
};

// Where the boot keeps the field its current card began on: a guest word, or a saved register of
// the logo sequence.
struct LogoHoldStart {
  std::uint32_t word = 0;
  unsigned reg = 0;
};

struct LogoHoldFacts {
  std::span<const LogoHold> holds;
  LogoHoldStart start;
  // The count a negative VSync returns.
  std::uint32_t vsyncCounter = 0;
};

// A Start/Cross press ends the hold the boot waits in now or next by dating its card's start
// `fields` back from the guest's vblank count, so the hold's own test exits and the card's
// fade-out runs at normal speed.
class LogoHoldSkip {
public:
  explicit LogoHoldSkip(const LogoHoldFacts &facts) : facts_(facts) {}

  // After each boot field, with the suspended boot call.
  void observe(Core &core, psx::cpu::ResumableGuestCall &call, bool pressed);

private:
  LogoHoldFacts facts_;
  PressLatch press_;
};

} // namespace spyro
