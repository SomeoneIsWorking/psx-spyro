#include "spyro1/render/sky_producer.h"

#include "core.h"
#include "core/session.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_working_memory.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <array>
#include <memory>
#include <vector>

namespace spyro1::sky {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

// The camera's view rotation, which the classification pass reads (terrain::kSharedFacts).
constexpr std::uint32_t kViewRotation = terrain::kSharedFacts.classify.viewRotation;

// The rotations the call's $a1 and $a2 name.
Rotations readRotations(Core &core) {
  const psx::present::EmitMemory guest(core);
  return Rotations{.view = spyro::guest_memory::rotationAt(guest, core.r[5]),
                   .projection = spyro::guest_memory::rotationAt(guest, core.r[6])};
}

void drawSky(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::uint32_t view = guest.r[5];
  const std::uint32_t projection = guest.r[6];
  const std::array<GuestSpan, 7> working =
      actors::binnedWorkingRanges(guest,
                                  GuestSpan{kOutcodes, kOutcodesBytes},
                                  GuestSpan{kSectorList, kSectorListBytes},
                                  GuestSpan{terrain::kPacketPoolEnd, 4},
                                  GuestSpan{terrain::kPoolExhausted, 4});
  const psx::present::Kept<psx::present::LevelReads>::Use reads(spyro::session(guest).drawReads);
  const psx::present::EmitMemory memory(guest, *reads, working);
  const SkyState state{
      .camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals),
      .rotations = readRotations(guest),
      .group = static_cast<std::int32_t>(guest.r[4]),
      .cameraRotations =
          view == kViewRotation && projection == kRenderGlobals.cameraRotation ? 1u : 0u,
  };
  Cyclorama sky(guest, memory, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  sky.draw(state.group, state.rotations);
  sky.end();
  memory::saveDraw(guest, state, reads->words());
}

// The rotations an in-between draws with: the camera's at `t` when the field drew with the camera's
// own, else the later field's.
Rotations rotationsBetween(Core &core, const SkyState &from, const SkyState &to, float t) {
  if (from.cameraRotations == 0 || to.cameraRotations == 0) {
    return to.rotations;
  }
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core, terrain::kTerrainImage, from.camera, to.camera, t);
  return Rotations{.view = camera.viewRotation, .projection = camera.rotation};
}

} // namespace

void SkyProducer::render(const psx::present::StateInterval &states,
                         psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<SkyState>(states.from, "sky draw");
  const auto after = memory::unpackDraw<SkyState>(states.to, "sky draw");
  const Rotations rotations = rotationsBetween(core_, before.header, after.header, states.t);
  actors::InBetween between(core_, after.reads);
  between.host().zero(kOutcodes, kOutcodesBytes);
  between.host().zero(kSectorList, kSectorListBytes);
  between.mapTable();
  between.host().zero(terrain::kPoolExhausted, 4);
  Cyclorama sky(
      core_, between.memory(), spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  const std::optional<SkyChain> chain = sky.draw(after.header.group, rotations);
  if (!chain) {
    return;
  }
  psx::present::DrawModeTracker drawMode;
  between.emitChain(
      chain->first, psx::present::OtSlot{spyro::depth_bins::kTable, kSkyBin}, drawMode, sink);
}

void registerSkyOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 SkyProducer::kEntry,
                                 "spyro1-sky",
                                 drawSky,
                                 std::make_unique<SkyProducer>(core),
                                 psx::present::Arg::None);
}

} // namespace spyro1::sky
