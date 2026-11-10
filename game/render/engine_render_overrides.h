// engine_render_overrides.h — the render overrides Spyro 2 and 3 share: the terrain drawer, the
// moby visibility walk, the sky drawer, the flatten's depth bins and the whole-screen overlays,
// bound to one image's facts.
#pragma once

#include "core.h"
#include "native_dispatch.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_sky_drawer.h"
#include "render/screen_overlays.h"
#include "render/terrain/engine_terrain_image.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_drawer.h"
#include "render/terrain/guest_terrain_facts.h"

#include <algorithm>
#include <cstdint>
#include <span>

namespace spyro {

// What one image says about the routines it shares with the other.
struct EngineRenderFacts {
  guest_render_globals::Globals globals;
  guest_terrain::Facts terrain;
  guest_moby::Facts moby;
  guest_sky::Facts sky;
  std::uint32_t flatten = 0;
  const char *flattenName = "";
  // Flatten calls, by return address, whose pass is chained after the first and keeps no bins.
  std::span<const std::uint32_t> unbinnedFlattens;
  screen_overlays::TintFacts tint;
  screen_overlays::LetterboxFacts letterbox;
};

namespace engine_render {

// An override is a plain `void (*)(Core *)`, so each image's facts are a template argument.
template <const EngineRenderFacts &facts>
inline constexpr guest_terrain::EngineTerrainImage kTerrainImage{facts.terrain, facts.globals};

template <const EngineRenderFacts &facts> void drawTerrain(Core *core) {
  guest_terrain::draw(*core, kTerrainImage<facts>);
}

template <const EngineRenderFacts &facts> void drawSky(Core *core) {
  guest_sky::draw(*core, facts.sky, facts.terrain, facts.globals);
}

template <const EngineRenderFacts &facts> void walkMobys(Core *core) {
  guest_moby::walk(*core, facts.moby, facts.globals);
}

template <const EngineRenderFacts &facts> void flatten(Core *core) {
  if (std::ranges::find(facts.unbinnedFlattens, core->r[31]) == facts.unbinnedFlattens.end()) {
    const depth_bins::Bins published = depth_bins::publishedBins(*core, facts.globals);
    depth_bins::assignBins(*core, published);
    depth_bins::assignOverlay(*core, published, depth_bins::overlayBins(*core, facts.globals));
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, facts.flatten);
}

} // namespace engine_render

template <const EngineRenderFacts &facts> void registerEngineRenderOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, facts.terrain.entry, facts.terrain.overrideName, engine_render::drawTerrain<facts>);
  psx::cpu::installNativeOverride(
      core, facts.moby.entry, facts.moby.overrideName, engine_render::walkMobys<facts>);
  psx::cpu::installNativeOverride(
      core, facts.sky.entry, facts.sky.overrideName, engine_render::drawSky<facts>);
  psx::cpu::installNativeOverride(
      core, facts.flatten, facts.flattenName, engine_render::flatten<facts>);
  screen_overlays::installLetterbox(
      core, facts.letterbox, screen_overlays::letterboxOverride<facts.letterbox>);
  screen_overlays::installTint(core, facts.tint, screen_overlays::tintOverride<facts.tint>);
}

} // namespace spyro
