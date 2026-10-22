// screen_overlays.h — SCUS_942.28's whole-screen overlays (game/render/screen_overlays.h): both
// link through AddPrimToOT 0x800168DC into the overlay pair
// (titles/spyro1/render/ordering_table.h).
#pragma once

#include "render/screen_overlays.h"
#include "spyro1/render/terrain_facts.h"

class Core;

namespace spyro1 {

// DrawFullscreenTint 0x800190D4: its POLY_F4 covers rows 8 to 0xE8.
inline constexpr spyro::screen_overlays::TintFacts kFullscreenTint{
    .entry = 0x800190D4u,
    .overrideName = "spyro1-fullscreen-tint",
    .top = 8,
    .bottom = 0xE8,
};

// DrawCinematicLetterbox 0x80018F30: steps 0x800756C0 toward 0 or 22, bars from the screen's top
// and up from row 0xF0.
inline constexpr spyro::screen_overlays::LetterboxFacts kCinematicLetterbox{
    .entry = 0x80018F30u,
    .overrideName = "spyro1-cinematic-letterbox",
    .height = 0x800756C0u,
    .primitiveCursor = terrain::kRenderGlobals.primitiveCursor,
    .top = 0,
    .bottom = 0xF0,
};

inline void registerScreenOverlayOverrides(Core &core) {
  namespace screen_overlays = spyro::screen_overlays;
  screen_overlays::installTint(
      core, kFullscreenTint, screen_overlays::tintOverride<kFullscreenTint>);
  screen_overlays::installLetterbox(
      core, kCinematicLetterbox, screen_overlays::letterboxOverride<kCinematicLetterbox>);
}

} // namespace spyro1
