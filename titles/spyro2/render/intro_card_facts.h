// intro_card_facts.h — SCUS_944.25's title intro card for spyro::intro_card.
#pragma once

#include "spyro2/core/boot_facts.h"
#include "render/field/intro_card.h"

#include <array>

namespace spyro2 {

// The intro card, mesh 3 of the title level (record 3): FUN_80044504, the moby drawer the walk
// hands its render list to (8004C564), decodes its frames with the delta words at 0x80062674
// (800445C0). Its front face, a thick board of 58 vertices with its post, has five rows; each pair is the
// row's outermost vertex on the left and right, matched against the drawer's packets at f400.
inline constexpr std::array<spyro::intro_card::Row, 5> kIntroCardRows{{{23, 41}, {22, 40}, {19, 37}, {18, 36}, {15, 42}}};

inline constexpr spyro::intro_card::Facts kIntroCard{
    .drawer = 0x80044504u,
    .overrideName = "spyro2-moby-drawer",
    .deltaTable = 0x80062674u,
    .gameState = kGameState,
    .titleGameState = kTitleScreenGameState,
    .mesh = 3u,
    .rows = kIntroCardRows,
};

} // namespace spyro2
