// test_ui_anchor.cpp — the horizontal anchoring rule: each class at 4:3 and 16:9, and each refusal.
#include "testutil.h"
#include "ui_anchor.h"

#include <optional>

namespace {

using spyro::ui_anchor::Anchor;
using spyro::ui_anchor::checkFrame;
using spyro::ui_anchor::correction;
using spyro::ui_anchor::Frame;
using spyro::ui_anchor::margin;
using spyro::ui_anchor::offset;
using spyro::ui_anchor::Refusal;

constexpr Frame k43{512, 512};
constexpr Frame k169{512, 684};

void test_4_3_is_the_identity_for_every_class() {
  CHECK_EQ(margin(k43), 0);
  for (const Anchor anchor : {Anchor::LeftEdge, Anchor::Centred, Anchor::RightEdge}) {
    CHECK(offset(anchor, k43) == std::optional<int>(0));
    CHECK(correction(anchor, k43) == std::optional<int>(0));
  }
}

void test_each_class_moves_by_its_share_of_the_margin() {
  CHECK_EQ(margin(k169), 86);
  CHECK(offset(Anchor::LeftEdge, k169) == std::optional<int>(0));
  CHECK(offset(Anchor::Centred, k169) == std::optional<int>(86));
  CHECK(offset(Anchor::RightEdge, k169) == std::optional<int>(172));
}

// The canvas already centres guest 2D by one margin; the correction is what is left per class.
void test_the_correction_undoes_the_centring_per_class() {
  CHECK(correction(Anchor::LeftEdge, k169) == std::optional<int>(-86));
  CHECK(correction(Anchor::Centred, k169) == std::optional<int>(0));
  CHECK(correction(Anchor::RightEdge, k169) == std::optional<int>(86));
  // A right-edge element 24 px in from the 4:3 edge stays 24 px in from the 16:9 edge.
  constexpr int kRightInset = 24;
  constexpr int kAuthoredRight = 512 - kRightInset;
  const std::optional<int> shift = correction(Anchor::RightEdge, k169);
  CHECK(shift.has_value());
  if (shift.has_value()) {
    CHECK_EQ(kAuthoredRight + margin(k169) + *shift, k169.drawn - kRightInset);
  }
}

void test_a_misread_frame_is_refused_and_not_moved() {
  CHECK(checkFrame(Frame{512, 400}) == Refusal::NarrowerFrame);
  CHECK(checkFrame(Frame{0, 684}) == Refusal::NonPositiveWidth);
  CHECK(checkFrame(Frame{512, 0}) == Refusal::NonPositiveWidth);
  CHECK(!correction(Anchor::LeftEdge, Frame{512, 400}).has_value());
  CHECK(!offset(Anchor::Centred, Frame{0, 684}).has_value());
}

} // namespace

int main() {
  RUN(4_3_is_the_identity_for_every_class);
  RUN(each_class_moves_by_its_share_of_the_margin);
  RUN(the_correction_undoes_the_centring_per_class);
  RUN(a_misread_frame_is_refused_and_not_moved);
  return pt_summary();
}
