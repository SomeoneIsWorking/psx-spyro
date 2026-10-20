// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/title_logo_facts.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {

// Sprite 0 of the title overlay's table at 0x8006FACC is the wordmark.
inline constexpr spyro::TitleLogoPart kLogoParts[] = {{.record = 0x8006FACCu}};

// The overlay draws the logo from m_CurrentTick 1169 (titlescreen.c:100-157).
inline constexpr spyro::TitleLogoFacts kLogoFacts{
    .valid = true,
    .gate = {.pointerGlobal = guest::kCutsceneLayout,
             .tickOffset = guest::kCutsceneLayoutCurrentTick,
             .tickValue = 1170u},
    .parts = kLogoParts,
};

} // namespace spyro1
