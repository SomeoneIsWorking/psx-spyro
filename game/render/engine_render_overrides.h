// engine_render_overrides.h — the render overrides Spyro 2 and 3 share: the terrain drawer, the
// moby visibility walk, the sky drawer, the flatten's depth bins, the whole-screen overlays and the
// world draws that test native depth, bound to one image's facts.
#pragma once

#include "core.h"
#include "native_dispatch.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_sky_drawer.h"
#include "render/field/spyro_drawer_depth.h"
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
  // The drawer of the walk's render list (SCUS_944.25 0x80044504, SCUS_944.67 0x80031124): each
  // entry's faces under the translation the walk tagged with its view unit and bucket.
  std::uint32_t mobyDrawer = 0;
  spyro_drawer_depth::Facts spyroDrawer;
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

template <const EngineRenderFacts &facts> void drawSpyro(Core *core) {
  spyro_drawer_depth::draw(*core, facts.spyroDrawer);
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
  psx::cpu::installNativeOverride(core,
                                  facts.spyroDrawer.entry,
                                  facts.spyroDrawer.overrideName,
                                  engine_render::drawSpyro<facts>);
  // A moby's faces keep its drawer's order and test at their biased buckets
  // (guest_moby_visibility.cpp).
  core.depthTest.mark(facts.terrain.entry,
                      psx::present::DepthOrder::Polygon,
                      guest_render_globals::kDepthBinViewUnits);
  core.depthTest.mark(facts.mobyDrawer, psx::present::DepthOrder::Model);
  core.vertexPlaces.sortFaces({.function = facts.mobyDrawer,
                               .faceLoad = guest_moby::kDrawerFaceLoad,
                               .slots = guest_moby::drawerFaceSlots});
  spyro_drawer_depth::markDepth(core, facts.spyroDrawer);
}

} // namespace spyro
