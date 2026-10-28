// screen_overlays.h — the whole-screen overlays all three images draw the same way, as state
// producers: the fullscreen tint (Spyro 1 0x800190D4, Spyro 2 0x8004D748, Spyro 3 0x800200A0) and
// the cinematic letterbox (0x80018F30, 0x8004D604, 0x8001FF44).
//
// Each override keys the packets its guest body writes by the call site, saves what the body draws
// from and runs the body. Both bodies link into the overlay pair, so their renders emit into that
// one slot. Inside a field redraw neither draws: their own renders stand for them at the
// in-between.
#pragma once

#include "native_dispatch.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace spyro::screen_overlays {

// The draw buffer's width, which the whole-screen overlays span.
inline constexpr std::uint32_t kScreenWidth = 0x200;

// DrawFullscreenTint($a0 blend, $a1 r, $a2 g, $a3 b): the blend as a draw mode, then one
// semi-transparent POLY_F4 512 wide over the rows `top` to `bottom`.
struct TintFacts {
  std::uint32_t entry = 0; // 0 when the image has none
  const char *overrideName = "";
  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
};

// The letterbox steps its bar height word toward 0 or its full height, then draws two opaque black
// POLY_F4 bars that tall, 512 wide, down from `top` and up from `bottom`.
struct LetterboxFacts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  std::uint32_t height = 0;
  std::uint32_t primitiveCursor = 0; // the bars are the next two packets it writes
  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
};

class FullscreenTint final : public psx::present::StateProducer {
public:
  struct State {
    std::uint32_t blend = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t pad = 0;
  };

  explicit FullscreenTint(const TintFacts &facts) : facts_(facts) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  // An additive or subtractive tint the guest skips (a fade at 0) is one of colour 0.
  [[nodiscard]] bool rests(std::span<const std::byte> drawn,
                           std::span<std::byte> into) const override;

private:
  const TintFacts &facts_;
};

class CinematicLetterbox final : public psx::present::StateProducer {
public:
  struct State {
    std::int32_t height = 0;
  };

  explicit CinematicLetterbox(const LetterboxFacts &facts) : facts_(facts) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  // The guest draws no bars at height 0.
  [[nodiscard]] bool rests(std::span<const std::byte> drawn,
                           std::span<std::byte> into) const override;

private:
  const LetterboxFacts &facts_;
};

// The override bodies. drawLetterbox returns whether the guest body ran to its end.
void drawTint(Core &core, const TintFacts &facts);
bool drawLetterbox(Core &core, const LetterboxFacts &facts);

// An override is a plain `void (*)(Core *)`, so each image's facts are a template argument.
template <const TintFacts &facts> void tintOverride(Core *core) {
  drawTint(*core, facts);
}

template <const LetterboxFacts &facts> void letterboxOverride(Core *core) {
  (void)drawLetterbox(*core, facts);
}

// The tint's override and its render.
void installTint(Core &core, const TintFacts &facts, psx::cpu::NativeFunction draw);
// The letterbox's render, for an override `draw` that runs drawLetterbox.
void installLetterbox(Core &core, const LetterboxFacts &facts, psx::cpu::NativeFunction draw);

} // namespace spyro::screen_overlays
