#pragma once

#include "core/title_logo_facts.h"
#include "spyro3/core/boot_facts.h"

#include <array>

namespace spyro3 {

// The wordmark is one 8bpp quad on the page at VRAM (512, 0) through the palette at (512, 0); the
// page also holds terrain.
inline constexpr std::array<psx::present::LogoTexture, 1> kLogoTextures{
    {{.page = {.x = 512, .y = 0}, .clut = psx::gpu::VramPoint{.x = 512, .y = 0}}}};

inline constexpr spyro::TitleLogoFacts kLogoFacts{
    .gate = {.gameStateGlobal = kGameState,
             .titleScreenState = kTitleScreenGameState,
             .subStateGlobal = kTitleState,
             .subStateMask = 0x00FF00FFu,
             .subStateValue = kTitleWaiting | (kTitleStepLogoUp << 16)},
    .textures = kLogoTextures,
};

} // namespace spyro3
