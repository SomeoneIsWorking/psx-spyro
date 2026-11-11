// screen_overlays.h — SCUS_942.28's cinematic letterbox (game/render/screen_overlays.h), linked
// through AddPrimToOT 0x800168DC into the overlay pair (titles/spyro1/render/ordering_table.h).
// DrawFullscreenTint 0x800190D4 is guest code: every draw that calls it is replayed.
#pragma once

#include "render/screen_overlays.h"
#include "spyro1/render/terrain_facts.h"

class Core;

namespace spyro1 {

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
  screen_overlays::installLetterbox(
      core, kCinematicLetterbox, screen_overlays::letterboxOverride<kCinematicLetterbox>);
}

} // namespace spyro1
