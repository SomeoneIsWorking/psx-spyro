#include "spyro1/render/portal_producer.h"

#include "blend.h"
#include "core.h"
#include "core/native_execution.h"
#include "render/field/guest_word.h"
#include "render/state_producer_override.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/portals.h"
#include "spyro1/render/sky_producer.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <array>
#include <memory>
#include <optional>

namespace spyro1::portals {
namespace {

namespace memory = spyro::guest_memory;
using psx::cpu::GuestCallSite;
using spyro::guest_camera::kTurnBits;
using spyro::guest_word::asSigned;
using terrain::kRenderGlobals;

constexpr std::string_view kOwner = "spyro1-portals";
constexpr GuestCallSite kLevelSky = GuestCallSite::fromJal(0x80051FB0u, sky::SkyProducer::kEntry);
// What the fill and the near sky write besides their packets: the outline, its window and depth,
// and the fade.
constexpr std::array<GuestSpan, 7> kScratch{{
    {kProjected, kProjectedBytes},
    {kEdges, kEdgesBytes},
    {kBin, 4},
    {kPieceQueue, 4},
    {kWindow, kWindowBytes},
    {kFadeStep, 4},
    {kFadeColour, 4},
}};

void drawPortals(Core *core) {
  Core &guest = *core;
  const spyro::PreservedReturnAddress returnAddress(guest);
  PortalState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals),
                    .spin = advanceSkySpin(guest),
                    .ticks = guest.mem_r32(guest::kLevelTicks)};
  const sky::Rotations rotations = nearSkyRotations(
      guest, {state.camera.angles[0], state.camera.angles[1], state.camera.angles[2]}, state.spin);
  const auto working =
      actors::binnedWorkingRanges(guest,
                                  GuestSpan{actors::kWorkBlock, actors::kWorkBlockBytes},
                                  GuestSpan{sky::kOutcodes, sky::kOutcodesBytes},
                                  kScratch[0],
                                  kScratch[1],
                                  kScratch[2],
                                  kScratch[3],
                                  kScratch[4],
                                  kScratch[5],
                                  kScratch[6]);
  psx::present::LevelReads reads;
  const psx::present::EmitMemory emit(guest, reads, working);
  Portals portals(guest, emit);
  const std::int32_t count = asSigned(guest.mem_r32(guest::kPortalCount));
  for (std::int32_t index = 0; index < count; ++index) {
    state.index = static_cast<std::uint32_t>(index);
    const auto scope = guest.emission.instance(guest.mem_r32(guest::kPortals + state.index * 4));
    reads.clear();
    if (const std::optional<Approach> approach = portals.drawFar(state.index)) {
      portals.drawNear(*approach, rotations);
    }
    memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
  }
  std::uint32_t group = guest.mem_r32(guest::kCameraOcclusionGroup);
  if (asSigned(group) >= asSigned(guest.mem_r32(guest::kEnvironmentOcclusionGroupCount))) {
    group = 0xFFFFFFFFu;
  }
  psx::cpu::callGuestNow(guest,
                         kOwner,
                         kLevelSky,
                         group,
                         terrain::kSharedFacts.classify.viewRotation,
                         kRenderGlobals.cameraRotation);
}

} // namespace

void PortalProducer::render(const psx::present::StateInterval &states,
                            psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<PortalState>(states.from, "portal draw");
  const auto after = memory::unpackDraw<PortalState>(states.to, "portal draw");
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  const spyro::guest_terrain::InBetweenCamera camera = spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t);
  between.mapCamera(camera);
  between.mapTable();
  between.provideWord(guest::kLevelTicks,
                      psx::present::lerpWord(before.header.ticks, after.header.ticks, states.t));
  for (const GuestSpan &span : kScratch) {
    between.host().zero(span.address, span.bytes);
  }
  const SkySpin spin{.yaw = psx::present::lerpTurn(
                         before.header.spin.yaw, after.header.spin.yaw, kTurnBits, states.t),
                     .pitch = psx::present::lerpTurn(
                         before.header.spin.pitch, after.header.spin.pitch, kTurnBits, states.t)};
  Portals portals(core_, between.memory());
  if (const std::optional<Approach> approach = portals.drawFar(after.header.index)) {
    portals.drawNear(
        *approach,
        nearSkyRotations(core_, {camera.angles[0], camera.angles[1], camera.angles[2]}, spin));
  }
  between.emitBins(sink);
}

void registerPortalOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 PortalProducer::kEntry,
                                 "spyro1-portals",
                                 drawPortals,
                                 std::make_unique<PortalProducer>(core));
}

} // namespace spyro1::portals
