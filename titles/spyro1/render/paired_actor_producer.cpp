#include "spyro1/render/paired_actor_producer.h"

#include "core.h"
#include "core/session.h"
#include "gte_control.h"
#include "native_dispatch.h"
#include "render/field/guest_render_globals.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/paired_actor.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <vector>

namespace spyro1::actors {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

// g_anSpyroHornStrikeAnchorPos + 0xB8: the five rotation words the routine may write there.
constexpr std::uint32_t kHornRotation = kHornAnchor + 0xB8u;
constexpr std::uint32_t kHornRotationBytes = 0x14u;

void drawSpyro(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::uint32_t cursor = guest.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = guest.mem_r32(terrain::kPacketPoolEnd);
  const std::array<memory::GuestRange, 6> working = {{
      {kVertexScreens, kScratchpadBytes},
      {kWorkBlock, kWorkBlockBytes},
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {guest.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {kRenderGlobals.primitiveCursor, 4},
      {kHornRotation, kHornRotationBytes},
  }};
  memory::GuestMemory reads(guest, working);
  PairedActorState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t at = 0; at < state.record.size(); ++at) {
    state.record[at] = guest.mem_r8(kSpyroRecord + at);
  }
  const auto scope = guest.emission.instance(kSpyroRecord);
  PairedActor actor(guest, reads);
  actor.draw();
  actor.end();
  const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
  guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
}

} // namespace

std::array<std::uint8_t, spyro_record::kBytes>
spyroBetween(const std::array<std::uint8_t, spyro_record::kBytes> &from,
             const std::array<std::uint8_t, spyro_record::kBytes> &to,
             float t) {
  std::array<std::uint8_t, spyro_record::kBytes> shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t at = spyro_record::kPosition + 4 * axis;
    storeWord(shown, at, wordBetween(loadWord(from, at), loadWord(to, at), t));
  }
  // Bytes 0..2 of each part's angle word; byte 3's sign says whether the part turns at all.
  for (std::uint32_t part = 0; part < 3; ++part) {
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
      const std::uint32_t at = spyro_record::kAngles + 4 * part + axis;
      shown[at] = angleBetween(from[at], to[at], t);
    }
  }
  return shown;
}

void PairedActorProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<PairedActorState>(states.from, "Spyro draw");
  const auto after = memory::unpackDraw<PairedActorState>(states.to, "Spyro draw");
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core_,
                                          terrain::kTerrainImage,
                                          before.header.camera,
                                          after.header.camera,
                                          static_cast<double>(states.t));
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  host.overlay(after.reads);
  const std::uint32_t table = mapBinnedWorkspace(host, arena, camera);
  const std::array<std::uint8_t, spyro_record::kBytes> record =
      spyroBetween(before.header.record, after.header.record, states.t);
  host.mapEmpty(kSpyroRecord, spyro_record::kBytes);
  for (std::uint32_t at = 0; at < record.size(); ++at) {
    host.w8(kSpyroRecord + at, record[at]);
  }
  host.mapEmpty(kHornRotation, kHornRotationBytes);

  const psx::present::GteGuard gte;
  PairedActor actor(core_, host);
  if (!actor.draw()) {
    return;
  }
  emitDepthBins(host, arena, table, sink, "spyrodraw");
}

void registerPairedActorOverrides(Core &core) {
  spyro::session(core).stateProducers.install(PairedActorProducer::kEntry,
                                              std::make_unique<PairedActorProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  PairedActorProducer::kEntry,
                                  "spyro1-paired-actor",
                                  drawSpyro,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::actors
