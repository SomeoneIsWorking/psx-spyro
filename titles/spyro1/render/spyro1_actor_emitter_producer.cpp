#include "spyro1_actor_emitter_producer.h"

#include "core.h"
#include "gte_control.h"
#include "guest_render_globals.h"
#include "guest_view_slope.h"
#include "native_dispatch.h"
#include "spyro1_actor_emitter.h"
#include "spyro1_actor_emitter_layout.h"
#include "spyro1_lit_actor_emitter.h"
#include "spyro1_terrain_facts.h"
#include "spyro1_terrain_image.h"
#include "spyro_session.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace spyro1::actors {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

std::unique_ptr<ActorEmitter>
emitterFor(const EmitterPass &pass, Core &core, memory::WorkingMemory &memory) {
  if (pass.lit) {
    return std::make_unique<LitActorEmitter>(
        core, memory, spyro::presentedBounds(core, spyro1::terrain::kSharedFacts));
  }
  return std::make_unique<ActorEmitter>(
      core, memory, spyro::presentedBounds(core, spyro1::terrain::kSharedFacts));
}

// The level words `record`'s list builder reads under the field's camera, which an in-between
// reruns: the builder over a copy of what it writes.
std::vector<memory::LevelWord>
listReads(Core &guest, const EmitterPass &pass, std::uint32_t record) {
  memory::HostMemory probe(guest);
  probe.keepLevelReads();
  probe.map(kWorkBlock, kWorkBlockBytes);
  probe.map(record, static_cast<std::uint32_t>(sizeof(MobyBytes)));
  probe.map(kShadowListEnd, 4);
  const psx::present::GteGuard gte;
  (void)listRecord(guest, probe, *pass.list, record);
  return probe.levelReads();
}

void emitList(Core &guest, const EmitterPass &pass) {
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::uint32_t cursor = guest.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = guest.mem_r32(terrain::kPacketPoolEnd);
  const std::array<memory::GuestRange, 5> working = {{
      {kVertexScreens, kScratchpadBytes},
      {kWorkBlock, kWorkBlockBytes},
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {guest.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {kRenderGlobals.primitiveCursor, 4},
  }};
  memory::GuestMemory reads(guest, working);
  const std::unique_ptr<ActorEmitter> emitter = emitterFor(pass, guest, reads);
  EmittedRecordState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  emitter->begin(guest.r[3]);
  for (std::uint32_t entry = kDrawList;; entry += entry::kBytes) {
    if (reads.r32(entry + entry::kFlags) == 0) {
      break;
    }
    state.record = reads.r32(entry + entry::kRecord);
    for (std::uint32_t at = 0; at < state.bytes.size(); ++at) {
      state.bytes[at] = guest.mem_r8(state.record + at);
    }
    const auto scope = guest.emission.instance(state.record);
    std::vector<memory::LevelWord> levelWords = listReads(guest, pass, state.record);
    reads.forgetReads();
    (void)emitter->draw(entry);
    const std::vector<memory::LevelWord> drawn = reads.firstReads();
    levelWords.insert(levelWords.end(), drawn.begin(), drawn.end());
    const std::vector<std::byte> saved = memory::packDraw(state, levelWords);
    guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
  }
  emitter->end();
}

template <const EmitterPass &Pass> void emitterOverride(Core *core) {
  emitList(*core, Pass);
}

template <const EmitterPass &Pass> void registerEmitter(Core &core) {
  spyro::session(core).stateProducers.install(Pass.entry,
                                              std::make_unique<ActorEmitterProducer>(core, Pass));
  psx::cpu::installNativeOverride(core,
                                  Pass.entry,
                                  Pass.overrideName,
                                  emitterOverride<Pass>,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace

void ActorEmitterProducer::render(const psx::present::StateInterval &states,
                                  psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<EmittedRecordState>(states.from, "moby draw");
  const auto after = memory::unpackDraw<EmittedRecordState>(states.to, "moby draw");
  const EmittedRecordState &state = after.header;
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core_,
                                          terrain::kTerrainImage,
                                          before.header.camera,
                                          state.camera,
                                          static_cast<double>(states.t));
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  host.overlay(after.reads);
  const std::uint32_t table = mapBinnedWorkspace(host, arena, camera);
  const MobyBytes record = recordBetween(before.header.bytes, state.bytes, states.t);
  host.mapEmpty(state.record, static_cast<std::uint32_t>(record.size()));
  for (std::uint32_t at = 0; at < record.size(); ++at) {
    host.w8(state.record + at, record[at]);
  }
  host.mapEmpty(kShadowListEnd, 4);
  host.w32(kShadowListEnd, kShadowList);

  const psx::present::GteGuard gte;
  if (!listRecord(core_, host, *pass_.list, state.record)) {
    return;
  }
  // The near detail patches the frame's near polygon list in place, and puts it back.
  const std::uint32_t frame = host.r32(kDrawList + entry::kFrame);
  const std::uint32_t nearPolygons = host.r32(frame + emitter::frame::kNearPolygons);
  host.map(nearPolygons, host.r32(nearPolygons) + 4);
  const std::unique_ptr<ActorEmitter> emitter = emitterFor(pass_, core_, host);
  emitter->begin(0);
  if (!emitter->draw(kDrawList)) {
    return;
  }
  emitDepthBins(host, arena, table, sink, "mobydraw");
}

void registerEmitterOverrides(Core &core) {
  registerEmitter<kRegularEmitter>(core);
  registerEmitter<kLitEmitter>(core);
}

} // namespace spyro1::actors
