// hud_slide.h — the HUD widgets' slide as a motion source. Each widget's record keeps a state byte
// and a slide value its drawers turn into the place of every icon and digit; an in-between places
// each element `t` of the way from where the earlier tick's slide put it to where the later's does,
// through the native overrides that compute those places.
#pragma once

#include "motion_set.h"
#include "native_dispatch.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro {

// The widget records: `slotCount` of `slotBytes` from `firstSlot`, each with its state byte at
// `stateOffset` (0 is not drawn) and its slide at `slideOffset`, a signed halfword or an unsigned
// byte.
struct HudSlots {
  std::uint32_t firstSlot = 0;
  std::uint32_t slotBytes = 0;
  std::uint32_t slotCount = 0;
  std::uint32_t stateOffset = 0;
  std::uint32_t slideOffset = 0;
  bool slideIsByte = false;
};

// Spyro 2's slide function f(slot, offset, y): while the slot's state is `slidingIn` or
// `slidingOut` it returns that state's byte table at (slide + offset) / 2, the sum clamped to
// 0..`lastIndex`; otherwise `restOffset`. Below `lowerHalfY` (y greater) the result is negated.
struct HudSlideFacts {
  const HudSlots *slots = nullptr;
  std::uint32_t function = 0;
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

  explicit HudSlide(const HudSlots *slots) : slots_(slots) {}

  [[nodiscard]] const char *name() const override {
    return "hud slides";
  }
  void sample(Core &core) override;
  // Counts the drawn and unpaired slots; the overrides blend them.
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  struct Sample {
    std::uint8_t state = 0;
    std::int32_t slide = 0;
  };
  // The record at `slot` as it is now.
  [[nodiscard]] Sample now(Core &core, std::uint32_t slot) const;
  // The earlier tick's sample of `slot` while it is drawn in both samples and now.
  [[nodiscard]] std::optional<Sample> earlier(Core &core, std::uint32_t slot) const;

  struct Place {
    std::int32_t x = 0;
    std::int32_t y = 0;
  };
  struct Remainder {
    float x = 0.0f;
    float y = 0.0f;
  };
  // `t` of the way from `from` to `to`, rounded; what the rounding dropped is kept for the one
  // element the caller at this $sp draws next inside this replay run.
  [[nodiscard]] Place between(const Core &core, Place from, Place to, float t);
  // Drops any kept remainder: the caller's next element lies on its whole place.
  void settle();
  // The remainder kept for the element drawn now, once, when the emitter runs at the $sp of the
  // call that kept it inside the same replay run; zero otherwise.
  [[nodiscard]] Remainder takeRemainder(const Core &core);

private:
  [[nodiscard]] std::optional<std::size_t> slotIndex(std::uint32_t slot) const;
  [[nodiscard]] Sample read(Core &core, std::uint32_t slot) const;

  const HudSlots *slots_;
  std::array<Sample, kMaxSlots> from_{};
  std::array<Sample, kMaxSlots> to_{};
  struct Kept {
    Remainder place;
    std::uint32_t run = 0; // the replay's rollback run
    std::uint32_t sp = 0;
  };
  Kept kept_;
};

// Spyro 2's slide function override: the frame driver's HudSlide blend at the replay's fraction.
void slideFunction(Core *core, const HudSlideFacts &facts);

template <const HudSlideFacts &Facts> void installHudSlide(Core &core) {
  psx::cpu::installNativeOverride(core, Facts.function, "spyro-hud-slide", [](Core *c) {
    slideFunction(c, Facts);
  });
}

} // namespace spyro
