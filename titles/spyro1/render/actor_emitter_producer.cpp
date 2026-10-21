#include "spyro1/render/actor_emitter_producer.h"

#include "core.h"
#include "core/session.h"
#include "gte_control.h"
#include "guest_memory_map.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "render/state_producer_override.h"
#include "spyro1/render/actor_emitter.h"
#include "spyro1/render/actor_emitter_layout.h"
#include "spyro1/render/lit_actor_emitter.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <array>
#include <memory>
#include <optional>
#include <span>

namespace spyro1::actors {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

// The pass's emitter, held in place.
class PassEmitter {
public:
  PassEmitter(const EmitterPass &pass,
              Core &core,
              const psx::present::EmitMemory &memory,
              psx::present::PrimitiveSink *places = nullptr) {
    const auto bounds = spyro::presentedBounds(core, spyro1::terrain::kSharedFacts);
    if (pass.lit) {
      emitter_ = &lit_.emplace(core, memory, bounds, places);
    } else {
      emitter_ = &plain_.emplace(core, memory, bounds, places);
    }
  }
  PassEmitter(const PassEmitter &) = delete;
  PassEmitter &operator=(const PassEmitter &) = delete;

  ActorEmitter *operator->() const {
    return emitter_;
  }

private:
  std::optional<ActorEmitter> plain_;
  std::optional<LitActorEmitter> lit_;
  ActorEmitter *emitter_ = nullptr;
};

// Notes in `probe` the level words `record`'s list builder reads under the field's camera, which an
// in-between reruns: the builder over a copy of what it writes.
void listReads(Core &guest,
               const EmitterPass &pass,
               std::uint32_t record,
               psx::present::HostMemory &probe) {
  probe.keepLevelReads();
  probe.mapGuest(guest, kWorkBlock, kWorkBlockBytes);
  probe.mapGuest(guest, record, static_cast<std::uint32_t>(sizeof(spyro::moby_record::Bytes)));
  probe.mapGuest(guest, kShadowListEnd, 4);
  const psx::present::GteGuard gte(guest.gte());
  (void)listRecord(guest, psx::present::EmitMemory(guest, probe), *pass.list, record);
}

void emitList(Core &guest, const EmitterPass &pass) {
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<GuestSpan, 5> working =
      binnedWorkingRanges(guest,
                          GuestSpan{kVertexScreens, psx::kScratchpadBytes},
                          GuestSpan{kWorkBlock, kWorkBlockBytes});
  spyro::Session &session = spyro::session(guest);
  const psx::present::Kept<psx::present::LevelReads>::Use reads(session.drawReads);
  const psx::present::EmitMemory memory(guest, *reads, working);
  const PassEmitter emitter(pass, guest, memory);
  EmittedRecordState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  emitter->begin(guest.r[3]);
  for (std::uint32_t entry = kDrawList;; entry += entry::kBytes) {
    if (memory.mem_r32(entry + entry::kFlags) == 0) {
      break;
    }
    state.record = memory.mem_r32(entry + entry::kRecord);
    guest.readBytes(state.record, std::as_writable_bytes(std::span(state.bytes)));
    const auto scope = guest.emission.instance(state.record);
    const psx::present::Kept<psx::present::HostMemory>::Use probe(session.renderMemory);
    listReads(guest, pass, state.record, *probe);
    reads->clear();
    (void)emitter->draw(entry);
    memory::saveDraw(guest, state, probe->levelReads().words(), reads->words());
  }
  emitter->end();
}

template <const EmitterPass &Pass> void emitterOverride(Core *core) {
  emitList(*core, Pass);
}

template <const EmitterPass &Pass> void registerEmitter(Core &core) {
  spyro::installProducerOverride(core,
                                 Pass.entry,
                                 Pass.overrideName,
                                 emitterOverride<Pass>,
                                 std::make_unique<ActorEmitterProducer>(core, Pass));
}

} // namespace

void ActorEmitterProducer::render(const psx::present::StateInterval &states,
                                  psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<EmittedRecordState>(states.from, "moby draw");
  const auto after = memory::unpackDraw<EmittedRecordState>(states.to, "moby draw");
  const EmittedRecordState &state = after.header;
  const spyro::guest_terrain::InBetweenCamera camera = spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t);
  InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(camera);
  between.mapTable();
  between.provide(state.record,
                  recordBetween(before.header.bytes, state.bytes, states.t, &between.memory()));
  between.provideWord(kShadowListEnd, kShadowList);
  const psx::present::EmitMemory &memory = between.memory();
  if (!listRecord(core_, memory, *pass_.list, state.record)) {
    return;
  }
  // The near detail patches the frame's near polygon list in place, and puts it back.
  const std::uint32_t frame = memory.mem_r32(kDrawList + entry::kFrame);
  const std::uint32_t nearPolygons = memory.mem_r32(frame + emitter::frame::kNearPolygons);
  between.host().mapGuest(core_, nearPolygons, memory.mem_r32(nearPolygons) + 4);
  const PassEmitter emitter(pass_, core_, memory, &sink);
  emitter->begin(0);
  if (emitter->draw(kDrawList)) {
    between.emitBins(sink);
  }
}

void registerEmitterOverrides(Core &core) {
  registerEmitter<kRegularEmitter>(core);
  registerEmitter<kLitEmitter>(core);
}

} // namespace spyro1::actors
