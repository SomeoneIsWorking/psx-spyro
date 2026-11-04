// intro_card_facts.h — SCUS_944.25's title intro card for spyro::intro_card.
#pragma once

#include "render/field/intro_card.h"
#include "spyro2/core/boot_facts.h"

#include <array>

namespace spyro2 {

// Mesh 3 (record 3); each row is its front face's outermost left and right vertex.
inline constexpr std::array<spyro::intro_card::Row, 5> kIntroCardRows{
    {{23, 41}, {22, 40}, {19, 37}, {18, 36}, {15, 42}}};

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
