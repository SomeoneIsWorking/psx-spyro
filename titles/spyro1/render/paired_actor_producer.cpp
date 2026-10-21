#include "spyro1/render/paired_actor_producer.h"

#include "core.h"
#include "guest_memory_map.h"
#include "render/state_producer_override.h"
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
  const std::array<GuestSpan, 6> working =
      binnedWorkingRanges(guest,
                          GuestSpan{kVertexScreens, psx::kScratchpadBytes},
                          GuestSpan{kWorkBlock, kWorkBlockBytes},
                          GuestSpan{kHornRotation, kHornRotationBytes});
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  PairedActorState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(kSpyroRecord, std::as_writable_bytes(std::span(state.record)));
  const auto scope = guest.emission.instance(kSpyroRecord);
  PairedActor actor(guest, memory);
  actor.draw();
  actor.end();
  memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
}

} // namespace

std::array<std::uint8_t, spyro_record::kBytes> spyroBetween(const std::array<std::uint8_t, spyro_record::kBytes> &from,
                                                            const std::array<std::uint8_t, spyro_record::kBytes> &to,
                                                            float t) {
  std::array<std::uint8_t, spyro_record::kBytes> shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    wordBetween(shown, from, to, spyro_record::kPosition + 4 * axis, t);
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
  InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t));
  between.mapTable();
  between.provide(kSpyroRecord, spyroBetween(before.header.record, after.header.record, states.t));
  between.host().zero(kHornRotation, kHornRotationBytes);
  PairedActor actor(core_, between.memory());
  if (actor.draw()) {
    between.emitBins(sink);
  }
}

void registerPairedActorOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 PairedActorProducer::kEntry,
                                 "spyro1-paired-actor",
                                 drawSpyro,
                                 std::make_unique<PairedActorProducer>(core));
}

} // namespace spyro1::actors
