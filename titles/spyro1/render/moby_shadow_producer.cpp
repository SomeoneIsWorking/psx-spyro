#include "spyro1/render/moby_shadow_producer.h"

#include "blend.h"
#include "core.h"
#include "render/field/guest_view_slope.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/moby_shadows.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <memory>

namespace spyro1::shadows {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

void drawShadows(Core *core) {
  Core &guest = *core;
  const std::array<GuestSpan, 4> working =
      actors::binnedWorkingRanges(guest, GuestSpan{kStaging, kStagingBytes});
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  MobyShadows shadows(guest, memory, spyro::presentedBounds(guest, terrain::kSharedFacts));
  shadows.begin();
  MobyShadowState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  const std::uint32_t end = guest.mem_r32(actors::kShadowListEnd);
  for (std::uint32_t entry = actors::kShadowList; entry != end; entry += 8) {
    state.record = guest.mem_r32(entry);
    state.radius = guest.mem_r32(entry + 4);
    guest.readBytes(state.record, std::as_writable_bytes(std::span(state.bytes)));
    const auto scope = guest.emission.instance(state.record);
    reads.clear();
    shadows.draw(state.record, state.radius);
    memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
  }
  shadows.end();
}

// The record at `t`: position, angles and ground height moved, everything else `to`'s.
spyro::moby_record::Bytes shadowRecordBetween(const spyro::moby_record::Bytes &from,
                                              const spyro::moby_record::Bytes &to,
                                              float t) {
  spyro::moby_record::Bytes shown = actors::recordBetween(from, to, t);
  const auto fromGround = spyro::record_blend::load<std::uint32_t>(from, kGround);
  const auto toGround = spyro::record_blend::load<std::uint32_t>(to, kGround);
  if ((fromGround & 0xFFFFu) != 0 && (toGround & 0xFFFFu) != 0) {
    spyro::record_blend::store(
        shown,
        kGround,
        (toGround & 0xFFFF0000u) |
            psx::present::lerpHalf(fromGround, toGround, psx::present::Halfword::Low, t));
  }
  return shown;
}

} // namespace

void MobyShadowProducer::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<MobyShadowState>(states.from, "moby shadow");
  const auto after = memory::unpackDraw<MobyShadowState>(states.to, "moby shadow");
  const MobyShadowState &state = after.header;
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t));
  between.mapTable();
  between.provide(state.record, shadowRecordBetween(before.header.bytes, state.bytes, states.t));
  MobyShadows shadows(
      core_, between.memory(), spyro::presentedBounds(core_, terrain::kSharedFacts), &sink);
  shadows.begin();
  shadows.draw(state.record, psx::present::lerpWord(before.header.radius, state.radius, states.t));
  between.emitBins(sink);
}

void registerMobyShadowOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 MobyShadowProducer::kEntry,
                                 "spyro1-moby-shadows",
                                 drawShadows,
                                 std::make_unique<MobyShadowProducer>(core));
}

} // namespace spyro1::shadows
