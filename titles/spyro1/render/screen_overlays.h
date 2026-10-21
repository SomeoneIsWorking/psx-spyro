// screen_overlays.h — Spyro 1's whole-screen overlays as state producers: the fullscreen tint
// 0x800190D4 and the cinematic letterbox 0x80018F30.
//
// Each override keys the packets its guest body writes by the call site, saves what the body
// draws from and runs the body. Both bodies link through AddPrimToOT 0x800168DC into the overlay
// pair (titles/spyro1/render/ordering_table.h), so their renders emit into that one slot.
#pragma once

#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace spyro1 {

// DrawFullscreenTint($a0 blend, $a1 r, $a2 g, $a3 b): a DR_MODE carrying the blend, then one
// semi-transparent POLY_F4 over (0,8)-(512,232).
class FullscreenTint final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x800190D4u;

  struct State {
    std::uint32_t blend = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t pad = 0;
  };

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
};

// DrawCinematicLetterbox(): steps the bar height at 0x800756C0 toward 0 or 22, then draws two
// opaque black POLY_F4 bars that tall at the top and bottom of the 512x240 screen.
class CinematicLetterbox final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80018F30u;
  static constexpr std::uint32_t kBarHeight = 0x800756C0u;

  struct State {
    std::int32_t height = 0;
  };

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
};

// The overrides and their renders.
void registerScreenOverlayOverrides(Core &core);

} // namespace spyro1
