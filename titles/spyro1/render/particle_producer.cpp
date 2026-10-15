#include "spyro1/render/particle_producer.h"

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

namespace spyro1::particles {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

constexpr spyro::guest_render_globals::Globals kSpill{.registerSaveArea = kRegisterSpill};

std::uint16_t loadHalf(const RecordBytes &bytes, std::uint32_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8));
}

void storeHalf(RecordBytes &bytes, std::uint32_t at, std::uint16_t half) {
  bytes[at] = static_cast<std::uint8_t>(half);
  bytes[at + 1] = static_cast<std::uint8_t>(half >> 8);
}

// A coordinate halfword at `t` the short way, whether the kind reads it signed or not.
std::uint16_t halfBetween(std::uint16_t from, std::uint16_t to, float t) {
  const auto step = static_cast<std::int16_t>(static_cast<std::uint16_t>(to - from));
  return static_cast<std::uint16_t>(from + static_cast<std::uint16_t>(actors::wordBetween(
                                               0, static_cast<std::uint32_t>(step), t)));
}

void drawList(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kSpill);
  const std::array<memory::GuestRange, 3> binned = actors::binnedWorkingRanges(guest);
  const std::array<memory::GuestRange, 4> working = {
      {binned[0], binned[1], binned[2], {kPoolExhausted, 4}}};
  memory::GuestMemory reads(guest, working);
  EmitList list(guest, reads, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  list.begin();
  ParticleState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t at = guest.mem_r32(kListBase);; at += kRecordBytes) {
    actors::readBytes(guest, at, state.bytes);
    const auto scope = guest.emission.instance(at);
    reads.forgetReads();
    const Outcome outcome = list.draw(at);
    if (outcome == Outcome::End || outcome == Outcome::PoolFull) {
      list.end(outcome == Outcome::PoolFull);
      return;
    }
    if (outcome == Outcome::Drawn) {
      state.record = at;
      const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
      guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
    }
  }
}

} // namespace

RecordBytes particleBetween(const RecordBytes &from, const RecordBytes &to, float t) {
  RecordBytes shown = to;
  if (loadHalf(from, record::kKind) != loadHalf(to, record::kKind)) {
    return shown;
  }
  const std::uint32_t kind = to[record::kKind + 1];
  const bool line = kind == 1 || kind == 4;
  const std::uint32_t last = line ? record::kEnd + 4 : record::kPosition + 4;
  for (std::uint32_t at = record::kPosition; at <= last; at += 2) {
    storeHalf(shown, at, halfBetween(loadHalf(from, at), loadHalf(to, at), t));
  }
  if (kind == 2 || kind == 5 || kind >= 6) {
    shown[record::kAngle] = actors::angleBetween(from[record::kAngle], to[record::kAngle], t);
  }
  return shown;
}

void ParticleProducer::render(const psx::present::StateInterval &states,
                              psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<ParticleState>(states.from, "particle");
  const auto after = memory::unpackDraw<ParticleState>(states.to, "particle");
  const ParticleState &state = after.header;
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core_,
                                          terrain::kTerrainImage,
                                          before.header.camera,
                                          state.camera,
                                          static_cast<double>(states.t));
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  const std::uint32_t table = actors::mapBinnedWorkspace(host, arena, camera);
  host.mapEmpty(terrain::kPacketPoolEnd, 4);
  host.w32(terrain::kPacketPoolEnd, arena.base + arena.bytes);
  host.overlay(after.reads);
  actors::mapBytes(host, state.record, particleBetween(before.header.bytes, state.bytes, states.t));

  const psx::present::GteGuard gte;
  EmitList list(core_, host, spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  list.begin();
  if (list.draw(state.record) == Outcome::Drawn) {
    actors::emitDepthBins(host, arena, table, sink, "particles");
  }
}

void registerParticleOverrides(Core &core) {
  spyro::session(core).stateProducers.install(ParticleProducer::kEntry,
                                              std::make_unique<ParticleProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  ParticleProducer::kEntry,
                                  "spyro1-particles",
                                  drawList,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::particles
