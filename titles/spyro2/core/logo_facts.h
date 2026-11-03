#pragma once

#include "core/title_logo_facts.h"
#include "spyro2/core/boot_facts.h"

#include <array>

namespace spyro2 {

// The title screen's own state word; 2 holds the finished logo while "press start" waits.
inline constexpr std::uint32_t kTitleScreenState = 0x80067E0Cu;
inline constexpr std::uint32_t kTitleScreenWaiting = 2u;

// The wordmark plate is an 8bpp texture on the page at VRAM (512, 256).
inline constexpr std::array<psx::present::LogoTexture, 1> kLogoTextures{
    {{.page = {.x = 512, .y = 256}}}};

inline constexpr spyro::TitleLogoFacts kLogoFacts{
    .gate = {.gameStateGlobal = kGameState,
             .titleScreenState = kTitleScreenGameState,
             .subStateGlobal = kTitleScreenState,
             .subStateValue = kTitleScreenWaiting},
    .textures = kLogoTextures,
};

} // namespace spyro2
