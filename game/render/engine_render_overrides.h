// engine_render_overrides.h — the render overrides Spyro 2 and 3 share: the terrain drawer and the
// field's draw dispatcher as state producers, the moby visibility walk and the flatten's depth
// bins, bound to one image's facts.
#pragma once

#include "cd_stock_read_completion.h"
#include "core.h"
#include "core/native_execution.h"
#include "core/session.h"
#include "execution_control.h"
#include "guest_code_module.h"
#include "native_dispatch.h"
#include "render/field/field_redraw.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/field/loading_picture.h"
#include "render/screen_overlays.h"
#include "render/state_producer_override.h"
#include "render/terrain/engine_terrain_image.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/terrain_state_producer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace spyro {

// What one image says about the routines it shares with the other.
struct EngineRenderFacts {
  guest_render_globals::Globals globals;
  guest_terrain::Facts terrain;
  guest_moby::Facts moby;
  field_redraw::Facts field;
  std::uint32_t flatten = 0;
  const char *flattenName = "";
  // Flatten calls, by return address, whose pass is chained after the first and keeps no bins.
  std::span<const std::uint32_t> unbinnedFlattens;
  loading_picture::Facts loadingPicture;
  screen_overlays::TintFacts tint;
  // The dispatcher's last drawer, after which it links nothing.
  screen_overlays::LetterboxFacts letterbox;
};

namespace engine_render {

// An override is a plain `void (*)(Core *)`, so each image's facts are a template argument.
template <const EngineRenderFacts &facts>
inline constexpr guest_terrain::EngineTerrainImage kTerrainImage{facts.terrain, facts.globals};

template <const EngineRenderFacts &facts> void drawTerrain(Core *core) {
  guest_terrain::drawField(*core, kTerrainImage<facts>);
}

template <const EngineRenderFacts &facts>
inline constexpr field_redraw::Image kFieldImage{
    facts.field, kTerrainImage<facts>, facts.moby.firstRecordWord};

template <const EngineRenderFacts &facts> void redrawField(Core *core) {
  field_redraw::redrawField(*core, kFieldImage<facts>);
}

template <const EngineRenderFacts &facts> void endField(Core *core) {
  if (screen_overlays::drawLetterbox(*core, facts.letterbox)) {
    field_redraw::endField(*core, facts.globals);
  }
}

template <const EngineRenderFacts &facts, std::size_t screen> void redrawScreen(Core *core) {
  field_redraw::redrawScreen(*core, kFieldImage<facts>, facts.field.screenDraws[screen].entry);
}

// A screen draw `overlay` holds (0 the executable), keyed to its image; an overlay's key of the
// image it replaced is removed.
template <const EngineRenderFacts &facts, std::size_t screen>
void installScreenDraw(Core &core, std::uint64_t overlay) {
  const field_redraw::ScreenDraw &draw = facts.field.screenDraws[screen];
  if (draw.overlay != overlay) {
    return;
  }
  if (overlay == 0) {
    psx::cpu::installNativeOverride(
        core, draw.entry, draw.overrideName, redrawScreen<facts, screen>);
    return;
  }
  std::vector<psx::cpu::NativeKey> &installed = session(core).screenDrawOverrides;
  std::erase_if(installed, [&](const psx::cpu::NativeKey &key) {
    if (key.address != draw.entry) {
      return false;
    }
    (void)core.nativeDispatcher().remove(key);
    return true;
  });
  const std::optional<psx::cpu::NativeKey> key = psx::cpu::tryInstallNativeOverride(
      core, draw.entry, draw.overrideName, redrawScreen<facts, screen>);
  if (!key) {
    std::abort();
  }
  installed.push_back(*key);
}

// The screen draws `overlay` holds; 0 names the executable's.
template <const EngineRenderFacts &facts, std::size_t... screen>
void installScreenDraws(Core &core, std::uint64_t overlay, std::index_sequence<screen...>) {
  (installScreenDraw<facts, screen>(core, overlay), ...);
}

// A stock read landed as the image `content` names: the screen draws of the overlay it loaded.
template <const EngineRenderFacts &facts>
void screenDrawsLanded(Core &core,
                       const psx::cd::StockReadLanding &landing,
                       std::uint64_t content) {
  const bool holdsOne = std::ranges::any_of(facts.field.screenDraws, [&](const auto &draw) {
    return draw.overlay != 0 && draw.entry - landing.destination < landing.bytes;
  });
  if (!holdsOne) {
    return;
  }
  installScreenDraws<facts>(
      core, content, std::make_index_sequence<facts.field.screenDraws.size()>{});
}

template <const EngineRenderFacts &facts> void drawLoadingPicture(Core *core) {
  loading_picture::draw(*core, facts.loadingPicture, facts.globals);
}

template <const EngineRenderFacts &facts> void walkMobys(Core *core) {
  guest_moby::walk(*core, facts.moby, facts.globals);
}

template <const EngineRenderFacts &facts> void flatten(Core *core) {
  if (std::ranges::find(facts.unbinnedFlattens, core->r[31]) == facts.unbinnedFlattens.end()) {
    const depth_bins::Claim claim = facts.field.entry != 0
                                        ? field_redraw::fieldClaim(*core, facts.globals)
                                        : depth_bins::Claim{};
    depth_bins::assignBins(*core, depth_bins::publishedBins(*core, facts.globals), claim);
    depth_bins::assignBins(*core, depth_bins::overlayBins(*core, facts.globals), claim);
    if (facts.field.entry != 0) {
      field_redraw::saveField(*core, facts.globals);
    }
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, facts.flatten);
}

} // namespace engine_render

// The drawer's one caller passes $a1 = 0, its one object; $a0 is the scratch block's end, which
// alternates with the double-buffered packet pool.
template <const EngineRenderFacts &facts> void registerEngineRenderOverrides(Core &core) {
  installProducerOverride(core,
                          facts.terrain.entry,
                          facts.terrain.overrideName,
                          engine_render::drawTerrain<facts>,
                          std::make_unique<guest_terrain::TerrainStateProducer>(
                              core, engine_render::kTerrainImage<facts>),
                          psx::present::Arg::A1);
  psx::cpu::installNativeOverride(
      core, facts.moby.entry, facts.moby.overrideName, engine_render::walkMobys<facts>);
  if (facts.loadingPicture.entry != 0) {
    installProducerOverride(
        core,
        facts.loadingPicture.entry,
        facts.loadingPicture.overrideName,
        engine_render::drawLoadingPicture<facts>,
        std::make_unique<loading_picture::LoadingPicture>(core, facts.loadingPicture),
        psx::present::Arg::None);
  }
  if (facts.field.entry == 0) {
    psx::cpu::installNativeOverride(
        core, facts.flatten, facts.flattenName, engine_render::flatten<facts>);
    return;
  }
  psx::cpu::installNativeOverride(
      core, facts.field.entry, facts.field.overrideName, engine_render::redrawField<facts>);
  screen_overlays::installLetterbox(core, facts.letterbox, engine_render::endField<facts>);
  if (facts.tint.entry != 0) {
    screen_overlays::installTint(core, facts.tint, screen_overlays::tintOverride<facts.tint>);
  }
  engine_render::installScreenDraws<facts>(
      core, 0, std::make_index_sequence<facts.field.screenDraws.size()>{});
  installProducerOverride(core,
                          facts.flatten,
                          facts.flattenName,
                          engine_render::flatten<facts>,
                          std::make_unique<field_redraw::FieldRedrawProducer>());
}

} // namespace spyro
