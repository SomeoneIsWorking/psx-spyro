#include "spyro1/render/particle_producer.h"

#include "blend.h"
#include "core.h"
#include "core/session.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
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

void drawList(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kSpill);
  const std::array<GuestSpan, 4> working =
      actors::binnedWorkingRanges(guest, GuestSpan{terrain::kPoolExhausted, 4});
  const psx::present::Kept<psx::present::LevelReads>::Use reads(spyro::session(guest).drawReads);
  const psx::present::EmitMemory memory(guest, *reads, working);
  EmitList list(guest, memory, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  list.begin();
  ParticleState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t at = guest.mem_r32(kListBase);; at += kRecordBytes) {
    guest.readBytes(at, std::as_writable_bytes(std::span(state.bytes)));
    const auto scope = guest.emission.instance(at);
    reads->clear();
    const Outcome outcome = list.draw(at);
    if (outcome == Outcome::End || outcome == Outcome::PoolFull) {
      list.end(outcome == Outcome::PoolFull);
      return;
    }
    if (outcome == Outcome::Drawn) {
      state.record = at;
      memory::saveDraw(guest, state, reads->words());
    }
  }
}

// A record at `t`: its points and angle moved when both saves hold the same kind, else `to`.
RecordBytes particleBetween(const RecordBytes &from, const RecordBytes &to, float t) {
  RecordBytes shown = to;
  if (spyro::record_blend::load<std::uint16_t>(from, record::kKind) !=
      spyro::record_blend::load<std::uint16_t>(to, record::kKind)) {
    return shown;
  }
  const std::uint32_t kind = to[record::kKind + 1];
  const bool line = kind == 1 || kind == 4;
  const std::uint32_t last = line ? record::kEnd + 4 : record::kPosition + 4;
  // A coordinate halfword the short way, whether the kind reads it signed or not.
  for (std::uint32_t at = record::kPosition; at <= last; at += 2) {
    spyro::record_blend::store(shown,
                               at,
                               static_cast<std::uint16_t>(psx::present::lerpTurn(
                                   spyro::record_blend::load<std::uint16_t>(from, at),
                                   spyro::record_blend::load<std::uint16_t>(to, at),
                                   16,
                                   t)));
  }
  if (kind == 2 || kind == 5 || kind >= 6) {
    shown[record::kAngle] =
        spyro::record_blend::angleBetween(from[record::kAngle], to[record::kAngle], t);
  }
  return shown;
}

} // namespace

void ParticleProducer::render(const psx::present::StateInterval &states,
                              psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<ParticleState>(states.from, "particle");
  const auto after = memory::unpackDraw<ParticleState>(states.to, "particle");
  const ParticleState &state = after.header;
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t));
  between.mapTable();
  between.provide(state.record, particleBetween(before.header.bytes, state.bytes, states.t));
  EmitList list(
      core_, between.memory(), spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  list.begin();
  if (list.draw(state.record) == Outcome::Drawn) {
    between.emitBins(sink);
  }
}

void registerParticleOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 ParticleProducer::kEntry,
                                 "spyro1-particles",
                                 drawList,
                                 std::make_unique<ParticleProducer>(core));
}

} // namespace spyro1::particles
