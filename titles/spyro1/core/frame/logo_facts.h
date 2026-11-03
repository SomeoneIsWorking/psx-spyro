#pragma once

#include "core/title_logo_facts.h"
#include "spyro1/core/guest_globals.h"

#include <array>

namespace spyro1 {

// The wordmark (sprite 0 of the title overlay's table at 0x8006FACC, tpage 0x0098) is 8bpp on the
// page at VRAM (512, 256).
inline constexpr std::array<psx::present::LogoTexture, 1> kLogoTextures{
    {{.page = {.x = 512, .y = 256}}}};

// The overlay draws the logo from m_CurrentTick 1169 (titlescreen.c:100-157).
inline constexpr spyro::TitleLogoFacts kLogoFacts{
    .gate = {.gameStateGlobal = guest::kGamestate,
             .titleScreenState = guest::kGamestateTitleScreen,
             .pointerGlobal = guest::kCutsceneLayout,
             .tickOffset = guest::kCutsceneLayoutCurrentTick,
             .tickValue = 1170u},
    .textures = kLogoTextures,
};

} // namespace spyro1
