// ui_anchor.h — the one horizontal anchoring rule for the screen-space HUD on a widened canvas.
//
// The canvas adds margins around the guest's 512 columns, so guest 2D lands centred in the wider
// frame. That is right for a centred element only: a left-edge element must keep its inset from the
// left edge and a right-edge element from the right one. The class is a fact about the element,
// declared where the element is authored; the arithmetic is here. Nothing reads or writes guest
// memory.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

class Core;

namespace spyro::ui_anchor {

// The class of element, a property of the layout and never of the current aspect: an element that
// is edge-anchored at 4:3 is edge-anchored at 16:9.
enum class Anchor : std::uint8_t {
  // Authored distance to the LEFT edge is preserved: the drawn x equals the authored x.
  LeftEdge = 0,
  // The authored box keeps its distance to the frame's CENTRE: a panel, menu or caption block.
  Centred = 1,
  // Authored distance to the RIGHT edge is preserved: the element moves by the FULL margin, twice
  // what a centred one does.
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

// The two frame widths this policy relates: the width the guest authored its layout against, and
// the width this port presents into. They are equal at 4:3, which is what makes every answer below
// the identity there.
struct Frame {
  std::int32_t authored = 0;
  std::int32_t drawn = 0;
};

// Why a frame was refused: a zero width, or a drawn frame narrower than the authored one. A refused
// element is left where the guest put it.
enum class Refusal : std::uint8_t {
  None = 0,
  NonPositiveWidth,
  NarrowerFrame,
};

inline const char *refusalName(Refusal refusal) {
  switch (refusal) {
  case Refusal::None:
    return "none";
  case Refusal::NonPositiveWidth:
    return "non-positive-width";
  case Refusal::NarrowerFrame:
    return "narrower-frame";
  }
  return "unknown";
}

// A "widened" frame that is not wider is a misread width.
constexpr Refusal checkFrame(const Frame &frame) {
  if (frame.authored <= 0 || frame.drawn <= 0) {
    return Refusal::NonPositiveWidth;
  }
  if (frame.drawn < frame.authored) {
    return Refusal::NarrowerFrame;
  }
  return Refusal::None;
}

// The one horizontal margin: how much wider the drawn frame is than the authored one, halved. At
// 4:3 it is zero, and it is the unit the whole policy is written in — a centred element moves by
// one margin, a right-edge element by two.
constexpr std::int32_t margin(const Frame &frame) {
  return (frame.drawn - frame.authored) / 2;
}

// How far an anchored element's ORIGIN moves, in the drawn frame. `Centred` moves by the margin,
// `LeftEdge` not at all, `RightEdge` by the whole widening.
constexpr std::optional<std::int32_t> offset(Anchor anchor, const Frame &frame) {
  if (checkFrame(frame) != Refusal::None) {
    return std::nullopt;
  }
  switch (anchor) {
  case Anchor::LeftEdge:
    return 0;
  case Anchor::Centred:
    return margin(frame);
  case Anchor::RightEdge:
    return frame.drawn - frame.authored;
  }
  return std::nullopt;
}

// The correction for a guest x the canvas already centres: the element's own offset minus the
// margin centring applied.
constexpr std::optional<std::int32_t> correction(Anchor anchor, const Frame &frame) {
  const std::optional<std::int32_t> own = offset(anchor, frame);
  if (!own) {
    return std::nullopt;
  }
  return *own - margin(frame);
}

// One census line's element identity. `index` is the element's own ordinal within its family (the
// third life orb is index 2): a family name alone is not an element.
struct Element {
  const char *name;
  std::size_t index = 0;
};

// `correction`, logged; a refused element is corrected by zero and logged with its reason.
std::int32_t correctionAndReport(const Element &element, Anchor anchor, const Frame &frame);

// The frame a Core's screen-space UI anchors against: the presentation plan when it widens, and
// otherwise the title's authored window, which corrects by zero.
[[nodiscard]] Frame widescreenFrame(const Core &core, std::uint32_t authoredWidth);

} // namespace spyro::ui_anchor
