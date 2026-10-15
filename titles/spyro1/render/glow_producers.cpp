#include "spyro1/render/glow_producers.h"

#include "core.h"
#include "core/session.h"
#include "gte_control.h"
#include "native_dispatch.h"
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

// Host memory with the camera at `t` and an empty arena, table and cursor.
struct InBetween {
  memory::HostMemory host;
  memory::PacketArena arena;
  std::uint32_t table = 0;
};

void prepare(Core &core,
             InBetween &between,
             const spyro::guest_terrain::CameraState &from,
             const spyro::guest_terrain::CameraState &to,
             float t) {
  const spyro::guest_terrain::InBetweenCamera camera = spyro::guest_terrain::cameraBetween(
      core, terrain::kTerrainImage, from, to, static_cast<double>(t));
  between.arena = terrain::kTerrainImage.windows(core).inBetweenArena;
  between.table = actors::mapBinnedWorkspace(between.host, between.arena, camera);
}

void drawEmitters(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<memory::GuestRange, 3> working = actors::binnedWorkingRanges(guest);
  memory::GuestMemory reads(guest, working);
  GlowFans fans(guest, reads, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  fans.begin();
  EmitterState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t index = 0; index < kEmitterCount; ++index) {
    const std::uint32_t entry = kEmitters + index * kEmitterBytes;
    if (guest.mem_r32(entry + emitter::kCount) == 0) {
      continue;
    }
    state.emitter = entry;
    actors::readBytes(guest, entry, state.bytes);
    const std::uint32_t position = guest.mem_r32(entry + emitter::kPosition);
    for (std::uint32_t i = 0; i < 3; ++i) {
      state.position[i] = guest.mem_r32(position + i * 4);
    }
    const auto scope = guest.emission.instance(entry);
    reads.forgetReads();
    fans.draw(entry);
    const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
    guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
  }
  fans.end();
}

void drawSparkles(Core *core) {
  Core &guest = *core;
  const std::array<memory::GuestRange, 3> working = actors::binnedWorkingRanges(guest);
  memory::GuestMemory reads(guest, working);
  Sparkles sparkles(guest, reads, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  sparkles.begin(guest.r[4]);
  SparkleState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t index = 0; index < kSparkleCount; ++index) {
    const std::uint32_t record = kSparkles + index * kSparkleBytes;
    const auto scope = guest.emission.instance(record);
    reads.forgetReads();
    if (!sparkles.draw(record)) {
      continue;
    }
    state.record = record;
    actors::readBytes(guest, record, state.bytes);
    sparkles.retire(record);
    const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
    guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
  }
  sparkles.end();
}

} // namespace

std::array<std::uint8_t, kSparkleBytes>
sparkleBetween(const std::array<std::uint8_t, kSparkleBytes> &from,
               const std::array<std::uint8_t, kSparkleBytes> &to,
               float t) {
  std::array<std::uint8_t, kSparkleBytes> shown = to;
  for (std::uint32_t at = 0; at < 12; at += 4) {
    actors::storeWord(
        shown, at, actors::wordBetween(actors::loadWord(from, at), actors::loadWord(to, at), t));
  }
  shown[sparkle::kAngle] = actors::angleBetween(from[sparkle::kAngle], to[sparkle::kAngle], t);
  return shown;
}

void GlowFanProducer::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<EmitterState>(states.from, "glow emitter");
  const auto after = memory::unpackDraw<EmitterState>(states.to, "glow emitter");
  const EmitterState &state = after.header;
  InBetween between{.host = memory::HostMemory(core_)};
  prepare(core_, between, before.header.camera, state.camera, states.t);
  between.host.overlay(after.reads);
  actors::mapBytes(between.host, state.emitter, state.bytes);
  const std::uint32_t position = actors::loadWord(state.bytes, emitter::kPosition);
  between.host.mapEmpty(position, 12);
  for (std::uint32_t i = 0; i < 3; ++i) {
    between.host.w32(position + i * 4,
                     actors::wordBetween(before.header.position[i], state.position[i], states.t));
  }

  const psx::present::GteGuard gte;
  GlowFans fans(core_, between.host, spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  fans.begin();
  if (fans.draw(state.emitter)) {
    actors::emitDepthBins(between.host, between.arena, between.table, sink, "glows");
  }
}

void SparkleProducer::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<SparkleState>(states.from, "sparkle");
  const auto after = memory::unpackDraw<SparkleState>(states.to, "sparkle");
  const SparkleState &state = after.header;
  InBetween between{.host = memory::HostMemory(core_)};
  prepare(core_, between, before.header.camera, state.camera, states.t);
  between.host.overlay(after.reads);
  actors::mapBytes(
      between.host, state.record, sparkleBetween(before.header.bytes, state.bytes, states.t));

  const psx::present::GteGuard gte;
  Sparkles sparkles(
      core_, between.host, spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  sparkles.begin(0);
  if (sparkles.draw(state.record)) {
    actors::emitDepthBins(between.host, between.arena, between.table, sink, "sparkles");
  }
}

void registerGlowOverrides(Core &core) {
  auto &producers = spyro::session(core).stateProducers;
  producers.install(GlowFanProducer::kEntry, std::make_unique<GlowFanProducer>(core));
  producers.install(SparkleProducer::kEntry, std::make_unique<SparkleProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  GlowFanProducer::kEntry,
                                  "spyro1-glow-emitters",
                                  drawEmitters,
                                  psx::present::Producer{psx::present::Arg::A0});
  psx::cpu::installNativeOverride(core,
                                  SparkleProducer::kEntry,
                                  "spyro1-sparkles",
                                  drawSparkles,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::glows
