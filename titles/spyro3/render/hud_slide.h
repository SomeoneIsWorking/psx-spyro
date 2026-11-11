// hud_slide.h — Spyro 3's HUD widgets placed by their slide. A widget keeps its direction flags in
// byte +0x18 and its slide in byte +0x3F; FUN_80029708 moves the widget's icon by the slide table
// and hands its digit drawers (FUN_800291B8, FUN_800293C4) the signed slide each digit reads the
// table at. All three are native: inside a replay each element is placed between where the two
// ticks' slides put it (spyro::HudSlide).
#pragma once

#include <cstdint>

class Core;

namespace spyro3::hud_slide {

inline constexpr std::uint32_t kSlideStep = 0x80029708u;
inline constexpr std::uint32_t kValueDrawer = 0x800291B8u;
inline constexpr std::uint32_t kRatioDrawer = 0x800293C4u;

// The value drawer's body, called by the HUD anchor's override of it.
void drawValue(Core &core);

// After the icon emitter put its packets from `firstPacket` to the cursor, places their corners
// by what the slide's rounding dropped for this element.
void placeEmitted(Core &core, std::uint32_t firstPacket);

// The primitive cursor the icon emitter FUN_800289C8 builds its POLY_FT4s at.
[[nodiscard]] std::uint32_t primitiveCursor(Core &core);

void registerOverrides(Core &core);

} // namespace spyro3::hud_slide
