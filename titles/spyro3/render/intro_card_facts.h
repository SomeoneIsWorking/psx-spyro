// intro_card_facts.h — SCUS_944.67's title intro card for spyro::intro_card.
#pragma once

#include "spyro3/core/boot_facts.h"
#include "render/field/intro_card.h"

#include <array>

namespace spyro3 {

// The intro card, mesh 0 of the title level (record 0): FUN_80031124, the moby drawer the walk
// hands its render list to, decodes its frames with the delta words at 0x80066274. Its
// front face, a thick board of 92 vertices with its post, has six rows; each pair is the row's
// outermost vertex on the left and right.
inline constexpr std::array<spyro::intro_card::Row, 6> kIntroCardRows{{{55, 19}, {53, 23}, {43, 26}, {46, 12}, {44, 13}, {35, 0}}};

inline constexpr spyro::intro_card::Facts kIntroCard{
    .drawer = 0x80031124u,
    .overrideName = "spyro3-moby-drawer",
    .deltaTable = 0x80066274u,
    .gameState = kGameState,
    .titleGameState = kTitleScreenGameState,
    .mesh = 0u,
    .rows = kIntroCardRows,
};

} // namespace spyro3
