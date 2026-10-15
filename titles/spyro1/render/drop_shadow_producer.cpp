#include "spyro1/render/drop_shadow_producer.h"

#include "core.h"
#include "core/session.h"
#include "gte_control.h"
#include "native_dispatch.h"
#include "render/field/guest_render_globals.h"
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
  const std::array<memory::GuestRange, 3> binned = actors::binnedWorkingRanges(guest);
  const std::array<memory::GuestRange, 4> working = {
      {{kStaging, kStagingBytes}, binned[0], binned[1], binned[2]}};
  memory::GuestMemory reads(guest, working);
  ShadowState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  actors::readBytes(guest, kSpyroRecord, state.spyro);
  actors::readBytes(guest, shadow_block::kBlock, state.shadow);
  const auto scope = guest.emission.instance(shadow_block::kBlock);
  DropShadow shadow(guest, reads);
  shadow.draw();
  shadow.end();
  const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
  guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
}

} // namespace

std::array<std::uint8_t, shadow_block::kBytes>
shadowBetween(const std::array<std::uint8_t, shadow_block::kBytes> &from,
              const std::array<std::uint8_t, shadow_block::kBytes> &to,
              float t) {
  std::array<std::uint8_t, shadow_block::kBytes> shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t at = (shadow_block::kAnchor - shadow_block::kBlock) + 4 * axis;
    actors::storeWord(
        shown, at, actors::wordBetween(actors::loadWord(from, at), actors::loadWord(to, at), t));
  }
  return shown;
}

void DropShadowProducer::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<ShadowState>(states.from, "shadow draw");
  const auto after = memory::unpackDraw<ShadowState>(states.to, "shadow draw");
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core_,
                                          terrain::kTerrainImage,
                                          before.header.camera,
                                          after.header.camera,
                                          static_cast<double>(states.t));
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  const std::uint32_t table = actors::mapBinnedWorkspace(host, arena, camera);
  host.overlay(after.reads);
  actors::mapBytes(
      host, kSpyroRecord, actors::spyroBetween(before.header.spyro, after.header.spyro, states.t));
  actors::mapBytes(host,
                   shadow_block::kBlock,
                   shadowBetween(before.header.shadow, after.header.shadow, states.t));

  const psx::present::GteGuard gte(core_.gte());
  DropShadow shadow(core_, host);
  if (shadow.draw()) {
    actors::emitDepthBins(host, arena, table, sink, "shadow");
  }
}

void registerDropShadowOverrides(Core &core) {
  spyro::session(core).stateProducers.install(DropShadowProducer::kEntry,
                                              std::make_unique<DropShadowProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  DropShadowProducer::kEntry,
                                  "spyro1-drop-shadow",
                                  drawShadow,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::shadow
