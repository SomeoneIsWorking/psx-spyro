// hud_slide.h — the HUD counters' slide as a motion source. Each counter is an 8-byte slot (state
// byte, slide halfword +2); its drawers place every icon and digit at the offset the guest's
// slide function returns for the slot at that element's own index offset, so an in-between blends
// the returned offset per call, through the function's native override.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro {

// The slide function f(slot, offset, y): while the slot's state is `slidingIn` or `slidingOut` it
// returns that state's byte table at (slide + offset) / 2, the sum clamped to 0..`lastIndex`;
// otherwise `restOffset`. Below `lowerHalfY` (y greater) the result is negated.
struct HudSlideFacts {
  std::uint32_t function = 0;
  std::uint32_t firstSlot = 0;
  std::uint32_t slotBytes = 0;
  std::uint32_t slotCount = 0;
  std::uint8_t slidingIn = 0;
  std::uint8_t slidingOut = 0;
  std::uint32_t inTable = 0;
  std::uint32_t outTable = 0;
  std::int32_t lastIndex = 0;
  std::int32_t restOffset = 0;
  std::int32_t lowerHalfY = 0;
};

class HudSlide final : public psx::present::MotionSource {
public:
  static constexpr std::size_t kMaxSlots = 8;

  explicit HudSlide(const HudSlideFacts *facts) : facts_(facts) {}

  [[nodiscard]] const char *name() const override {
    return "hud slides";
  }
  void sample(Core &core) override;
  // Counts the drawn and unpaired slots; the override blends them.
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // One call's arguments: the slot, the element's index offset and its y.
  struct Call {
    std::uint32_t slot = 0;
    std::int32_t index = 0;
    std::int32_t y = 0;
  };
  // What the slide function returns for the call now, or, inside a replay at `t` with the slot
  // drawn in both samples, `t` of the way from what it returned for the earlier sample, rounded;
  // what the rounding dropped is kept for the element the caller draws next.
  [[nodiscard]] std::int32_t offset(Core &core, const Call &call, std::optional<float> t);
  // The rows the last offset's rounding dropped, once, inside the replay run that took it: each
  // caller adds the offset to the y of the one element it draws next (0x8005251C, 0x80052690,
  // 0x80052B88), with no call between.
  [[nodiscard]] float takeRemainder(const Core &core);

private:
  using Slots = std::array<std::uint32_t, kMaxSlots>; // each slot's first word
  [[nodiscard]] std::int32_t offsetOf(Core &core, std::uint32_t slotWord, const Call &call) const;
  [[nodiscard]] std::optional<std::size_t> slotIndex(std::uint32_t slot) const;

  const HudSlideFacts *facts_;
  Slots from_{};
  Slots to_{};
  struct Remainder {
    float rows = 0.0f;
    std::uint32_t run = 0; // the replay's rollback run
  };
  Remainder remainder_;
};

// Installs the slide function's override, which returns the frame driver's HudSlide::offset at the
// replay's fraction.
void installHudSlide(Core &core, const HudSlideFacts &facts);

} // namespace spyro
