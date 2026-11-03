#pragma once

#include "core/title_logo_facts.h"
#include "spyro3/core/boot_facts.h"

#include <array>

namespace spyro3 {

// Title image data, resident whenever the game state is the title's: the state byte and its step
// byte.
inline constexpr std::uint32_t kTitleState = 0x8007F12Cu;
inline constexpr std::uint32_t kTitleStep = 0x8007F12Eu;
// 1 holds the finished logo while "press start" waits.
inline constexpr std::uint8_t kTitleWaiting = 1u;

// The wordmark is one 8bpp quad on the page at VRAM (512, 0) through the palette at (512, 0); the
// page also holds terrain.
inline constexpr std::array<psx::present::LogoTexture, 1> kLogoTextures{
    {{.page = {.x = 512, .y = 0}, .clut = psx::gpu::VramPoint{.x = 512, .y = 0}}}};

inline constexpr spyro::TitleLogoFacts kLogoFacts{
    .gate = {.gameStateGlobal = kGameState,
             .titleScreenState = kTitleScreenGameState,
             .subStateGlobal = kTitleState,
             .subStateMask = 0xFFu,
             .subStateValue = kTitleWaiting},
    .textures = kLogoTextures,
};

} // namespace spyro3
