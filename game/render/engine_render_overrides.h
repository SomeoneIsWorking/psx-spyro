// engine_render_overrides.h — the render overrides Spyro 2 and 3 share: the terrain drawer as a
// state producer, the moby visibility walk and the flatten's depth bins, bound to one image's
// facts.
#pragma once

#include "core.h"
#include "engine_terrain_image.h"
#include "execution_control.h"
#include "guest_depth_bins.h"
#include "guest_moby_visibility.h"
#include "guest_render_globals.h"
#include "guest_terrain_facts.h"
#include "native_dispatch.h"
#include "native_execution.h"
#include "spyro_session.h"
#include "terrain_state_producer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

namespace spyro {

// What one image says about the routines it shares with the other.
struct EngineRenderFacts {
  guest_render_globals::Globals globals;
  guest_terrain::Facts terrain;
  guest_moby::Facts moby;
  std::uint32_t flatten = 0;
  const char *flattenName = "";
  // Flatten calls, by return address, whose pass is chained after the first and keeps no bins.
  std::span<const std::uint32_t> unbinnedFlattens;
};

namespace engine_render {

// An override is a plain `void (*)(Core *)`, so each image's facts are a template argument.
template <const EngineRenderFacts &facts>
inline constexpr guest_terrain::EngineTerrainImage kTerrainImage{facts.terrain, facts.globals};

template <const EngineRenderFacts &facts> void drawTerrain(Core *core) {
  guest_terrain::drawField(*core, kTerrainImage<facts>);
}

template <const EngineRenderFacts &facts> void walkMobys(Core *core) {
  guest_moby::walk(*core, facts.moby, facts.globals);
}

template <const EngineRenderFacts &facts> void flatten(Core *core) {
  if (std::ranges::find(facts.unbinnedFlattens, core->r[31]) == facts.unbinnedFlattens.end()) {
    depth_bins::assignBins(*core, depth_bins::publishedBins(*core, facts.globals));
  }
  (void)psx::cpu::completeOrPropagate(
      *core,
      psx::cpu::callOriginal(*core, facts.flatten, psx::cpu::ExecutionBudget::currentTurn(*core)));
}

} // namespace engine_render

// The drawer's one caller passes $a1 = 0, its one object; $a0 is the scratch block's end, which
// alternates with the double-buffered packet pool.
template <const EngineRenderFacts &facts> void registerEngineRenderOverrides(Core &core) {
  session(core).stateProducers.install(facts.terrain.entry,
                                       std::make_unique<guest_terrain::TerrainStateProducer>(
                                           core, engine_render::kTerrainImage<facts>));
  psx::cpu::installNativeOverride(core,
                                  facts.terrain.entry,
                                  facts.terrain.overrideName,
                                  engine_render::drawTerrain<facts>,
                                  psx::present::Producer{psx::present::Arg::A1});
  psx::cpu::installNativeOverride(
      core, facts.moby.entry, facts.moby.overrideName, engine_render::walkMobys<facts>);
  psx::cpu::installNativeOverride(
      core, facts.flatten, facts.flattenName, engine_render::flatten<facts>);
}

} // namespace spyro
