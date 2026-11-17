// flyby_card.h — the flyby card (GS_TitleScreen in TSM_Demo, drawn by 0x8001E6B8) on a widened
// canvas: GamestateCutsceneTransition parks Spyro just past the 4:3 edges between passes, so his
// draw places him for the presented width.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace spyro1 {

// For one GamestateDraw: Spyro's offset across the card's view scaled by the widening, so the
// card's 4:3 path spans the canvas; retail's place is restored when the draw returns.
class FlybyCardPlacement {
public:
  explicit FlybyCardPlacement(Core &core);
  ~FlybyCardPlacement();
  FlybyCardPlacement(const FlybyCardPlacement &) = delete;
  FlybyCardPlacement &operator=(const FlybyCardPlacement &) = delete;
  FlybyCardPlacement(FlybyCardPlacement &&) = delete;
  FlybyCardPlacement &operator=(FlybyCardPlacement &&) = delete;

private:
  Core &core_;
  std::optional<std::uint32_t> retailX_;
};

} // namespace spyro1
