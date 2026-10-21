// ui_anchor.h — the one horizontal anchoring rule for the screen-space HUD on a widened canvas: the canvas
// centres guest 2D, so a left-edge element moves back by the presentation margin and a right-edge one on by it.
#pragma once

#include <cstddef>
#include <cstdint>

class Core;

namespace spyro::ui_anchor {

// A property of the layout, never of the current aspect.
enum class Anchor : std::uint8_t {
  LeftEdge = 0,
  Centred = 1,
  RightEdge = 2,
};

inline const char *name(Anchor anchor) {
  switch (anchor) {
  case Anchor::LeftEdge:
    return "left-edge";
  case Anchor::Centred:
    return "centred";
  case Anchor::RightEdge:
    return "right-edge";
  }
  return "unknown";
}

// One element's identity in the log: its family and its ordinal within it.
struct Element {
  const char *name;
  std::size_t index = 0;
};

// The columns a guest x of `anchor` moves on the presented frame; zero when the plan does not widen.
std::int32_t correction(const Core &core, const Element &element, Anchor anchor);

// Moves the x argument in register `reg` by `correction`.
void shiftArgument(Core &core, std::uint32_t reg, const Element &element, Anchor anchor);

} // namespace spyro::ui_anchor
