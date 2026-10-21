#include "spyro1/render/drop_shadow_producer.h"

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/paired_actor_producer.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <vector>

namespace spyro1::shadow {
namespace {

namespace memory = spyro::guest_memory;
using actors::kSpyroRecord;
using terrain::kRenderGlobals;

void drawShadow(Core *core) {
  Core &guest = *core;
  const std::array<GuestSpan, 4> working =
      actors::binnedWorkingRanges(guest, GuestSpan{kStaging, kStagingBytes});
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  ShadowState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(kSpyroRecord, std::as_writable_bytes(std::span(state.spyro)));
  guest.readBytes(shadow_block::kBlock, std::as_writable_bytes(std::span(state.shadow)));
  const auto scope = guest.emission.instance(shadow_block::kBlock);
  DropShadow shadow(guest, memory);
  shadow.draw();
  shadow.end();
  memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
}

// The shadow block at `t`: the ground point moved, everything else `to`'s.
std::array<std::uint8_t, shadow_block::kBytes>
shadowBetween(const std::array<std::uint8_t, shadow_block::kBytes> &from,
              const std::array<std::uint8_t, shadow_block::kBytes> &to,
              float t) {
  std::array<std::uint8_t, shadow_block::kBytes> shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    spyro::record_blend::wordBetween(
        shown, from, to, (shadow_block::kAnchor - shadow_block::kBlock) + 4 * axis, t);
  }
  return shown;
}

} // namespace

void DropShadowProducer::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<ShadowState>(states.from, "shadow draw");
  const auto after = memory::unpackDraw<ShadowState>(states.to, "shadow draw");
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t));
  between.mapTable();
  between.provide(kSpyroRecord,
                  actors::spyroBetween(before.header.spyro, after.header.spyro, states.t));
  between.provide(shadow_block::kBlock,
                  shadowBetween(before.header.shadow, after.header.shadow, states.t));
  DropShadow shadow(core_, between.memory());
  if (shadow.draw()) {
    between.emitBins(sink);
  }
}

void registerDropShadowOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 DropShadowProducer::kEntry,
                                 "spyro1-drop-shadow",
                                 drawShadow,
                                 std::make_unique<DropShadowProducer>(core));
}

} // namespace spyro1::shadow
