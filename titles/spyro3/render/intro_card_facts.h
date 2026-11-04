// intro_card_facts.h — SCUS_944.67's title intro card for spyro::intro_card.
#pragma once

#include "render/field/intro_card.h"
#include "spyro3/core/boot_facts.h"

#include <array>

namespace spyro3 {

// Mesh 0 (record 0); each row is its front face's outermost left and right vertex.
inline constexpr std::array<spyro::intro_card::Row, 6> kIntroCardRows{
    {{55, 19}, {53, 23}, {43, 26}, {46, 12}, {44, 13}, {35, 0}}};

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
