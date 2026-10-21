#include "spyro1/render/glow_producers.h"

#include "core.h"
#include "blend.h"
#include "render/state_producer_override.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <vector>

namespace spyro1::glows {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

void drawEmitters(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<GuestSpan, 3> working = actors::binnedWorkingRanges(guest);
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  GlowFans fans(guest, memory, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  fans.begin();
  EmitterState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t index = 0; index < kEmitterCount; ++index) {
    const std::uint32_t entry = kEmitters + index * kEmitterBytes;
    if (guest.mem_r32(entry + emitter::kCount) == 0) {
      continue;
    }
    state.emitter = entry;
    guest.readBytes(entry, std::as_writable_bytes(std::span(state.bytes)));
    guest.readBytes(guest.mem_r32(entry + emitter::kPosition), std::as_writable_bytes(std::span(state.position)));
    const auto scope = guest.emission.instance(entry);
    reads.clear();
    fans.draw(entry);
    memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
  }
  fans.end();
}

void drawSparkles(Core *core) {
  Core &guest = *core;
  const std::array<GuestSpan, 3> working = actors::binnedWorkingRanges(guest);
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  Sparkles sparkles(guest, memory, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  sparkles.begin(guest.r[4]);
  SparkleState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t index = 0; index < kSparkleCount; ++index) {
    const std::uint32_t record = kSparkles + index * kSparkleBytes;
    const auto scope = guest.emission.instance(record);
    reads.clear();
    if (!sparkles.draw(record)) {
      continue;
    }
    state.record = record;
    guest.readBytes(record, std::as_writable_bytes(std::span(state.bytes)));
    sparkles.retire(record);
    memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
  }
  sparkles.end();
}

// A sparkle at `t`: position and angle moved, everything else `to`'s.
std::array<std::uint8_t, kSparkleBytes> sparkleBetween(const std::array<std::uint8_t, kSparkleBytes> &from,
                                                       const std::array<std::uint8_t, kSparkleBytes> &to,
                                                       float t) {
  std::array<std::uint8_t, kSparkleBytes> shown = to;
  for (std::uint32_t at = 0; at < 12; at += 4) {
    actors::wordBetween(shown, from, to, at, t);
  }
  shown[sparkle::kAngle] = actors::angleBetween(from[sparkle::kAngle], to[sparkle::kAngle], t);
  return shown;
}

} // namespace

void GlowFanProducer::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<EmitterState>(states.from, "glow emitter");
  const auto after = memory::unpackDraw<EmitterState>(states.to, "glow emitter");
  const EmitterState &state = after.header;
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t));
  between.mapTable();
  between.provide(state.emitter, state.bytes);
  const std::uint32_t position = actors::load<std::uint32_t>(state.bytes, emitter::kPosition);
  for (std::uint32_t i = 0; i < 3; ++i) {
    between.provideWord(position + i * 4, psx::present::lerpWord(before.header.position[i], state.position[i], states.t));
  }
  GlowFans fans(core_, between.memory(), spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  fans.begin();
  if (fans.draw(state.emitter)) {
    between.emitBins(sink);
  }
}

void SparkleProducer::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<SparkleState>(states.from, "sparkle");
  const auto after = memory::unpackDraw<SparkleState>(states.to, "sparkle");
  const SparkleState &state = after.header;
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t));
  between.mapTable();
  between.provide(state.record, sparkleBetween(before.header.bytes, state.bytes, states.t));
  Sparkles sparkles(core_, between.memory(), spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  sparkles.begin(0);
  if (sparkles.draw(state.record)) {
    between.emitBins(sink);
  }
}

void registerGlowOverrides(Core &core) {
  spyro::installProducerOverride(
      core, GlowFanProducer::kEntry, "spyro1-glow-emitters", drawEmitters, std::make_unique<GlowFanProducer>(core));
  spyro::installProducerOverride(
      core, SparkleProducer::kEntry, "spyro1-sparkles", drawSparkles, std::make_unique<SparkleProducer>(core));
}

} // namespace spyro1::glows
