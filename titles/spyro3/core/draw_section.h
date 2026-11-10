// draw_section.h — SCUS_944.67's per-field draw body 0x8001E638 as a replay section: its state
// switch over 0x8006E344, and the second pass states 4 and 0xD flatten and chain after the first.
#pragma once

#include "core/draw_section.h"

namespace spyro3 {

inline constexpr spyro::DrawSectionFacts kDrawSectionFacts{
    .gate = 0x8006C718u,
    .currentBuffer = 0x8006C600u,
    .buffer0 = 0x8006FBFCu,
    .buffer1 = 0x8006FC70u,
    .primitiveCursor = 0x8006C664u,
    .binMark = 0x8006C7D4u,
    .orderingTable = 0x8006C634u,
    .scratchEnd = 0x8006C668u,
    .scratchSize = 0x8006C7DCu,
    .cameraBuilder = 0x8004F6C4u,
    .cameraAngles = 0x8006E03Cu,
    .viewRotation = 0x8006E00Cu,
    .projectionRotation = 0x8006DFF8u,
    .positionCopy = 0x8004F178u,
    .drawCameraPosition = 0x8006E02Cu,
    .cameraPosition = 0x8006E020u,
    .gameState = 0x8006E344u,
    .dispatcher = 0x8001E460u,
    .flatten = 0x8004E664u,
};

class DrawSection final : public spyro::DrawSection {
public:
  DrawSection() : spyro::DrawSection(kDrawSectionFacts) {}

private:
  // The loading screen, state 5: 0x80059038 waits in VSync at 0x8005927C (UpdateLoadingImage).
  [[nodiscard]] bool replaysState(std::uint32_t state) const override {
    return state != 5u;
  }
  [[nodiscard]] bool
  drawState(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t state) const override;
  [[nodiscard]] std::optional<std::uint32_t>
  flattenFrame(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t state) const override;
  // The pass `draw` links into the emptied bins with the other buffer's pool as scratch, flattened
  // and chained after `head` through the tail word.
  [[nodiscard]] bool secondPass(Core &core,
                                psx::present::GuestRedraw &redraw,
                                std::span<const std::uint32_t> drawers) const;
};

} // namespace spyro3
