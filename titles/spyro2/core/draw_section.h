// draw_section.h — SCUS_944.25's draw body 0x800156FC and its state switch over 0x800681C8.
#pragma once

#include "core/draw_section.h"

namespace spyro2 {

inline constexpr spyro::DrawSectionFacts kDrawSectionFacts{
    .gate = 0x800670C8u,
    .currentBuffer = 0x80066FC0u,
    .buffer0 = 0x80069928u,
    .buffer1 = 0x8006999Cu,
    .primitiveCursor = 0x80067030u,
    .binMark = 0x80067168u,
    .orderingTable = 0x80066FFCu,
    .scratchEnd = 0x80067034u,
    .scratchSize = 0x80067170u,
    .cameraBuilder = 0x8001C2F8u,
    .cameraAngles = 0x80067EC8u,
    .viewRotation = 0x80067E98u,
    .projectionRotation = 0x80067E84u,
    .positionCopy = 0x8001BDB0u,
    .drawCameraPosition = 0x80067EB8u,
    .cameraPosition = 0x80067EACu,
    .gameState = 0x800681C8u,
    .dispatcher = 0x800155A0u,
    .flatten = 0x8001B2A8u,
};

class DrawSection final : public spyro::DrawSection {
public:
  DrawSection() : spyro::DrawSection(kDrawSectionFacts) {}

private:
  [[nodiscard]] std::optional<std::string_view> declinesState(Core &core,
                                                              std::uint32_t state) const override;
  [[nodiscard]] bool
  drawState(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t state) const override;
};

} // namespace spyro2
