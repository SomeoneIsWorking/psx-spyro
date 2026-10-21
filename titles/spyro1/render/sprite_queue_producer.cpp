#include "spyro1/render/sprite_queue_producer.h"

#include "core.h"
#include "core/session.h"
#include "guest_memory_map.h"
#include "render/field/guest_render_globals.h"
#include "render/hud/ui_anchor.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/hud_anchor.h"
#include "spyro1/render/sprite_queue.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <utility>
#include <vector>

namespace spyro1::actors {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

using SavedRecord = memory::SavedDraw<QueuedRecordState>;

SavedRecord readRecord(std::span<const std::byte> bytes) {
  return memory::unpackDraw<QueuedRecordState>(bytes, "queue record");
}

void drawQueue(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<GuestSpan, 7> working =
      binnedWorkingRanges(guest,
                          GuestSpan{kVertexScreens, psx::kScratchpadBytes},
                          GuestSpan{kWorkBlock, kWorkBlockBytes},
                          GuestSpan{guest.mem_r32(terrain::kRenderGlobals.overlayPair), 8},
                          GuestSpan{kShadowListEnd, 4});
  const psx::present::Kept<psx::present::LevelReads>::Use reads(spyro::session(guest).drawReads);
  const psx::present::EmitMemory memory(guest, *reads, working);
  SpriteQueue queue(guest, memory, presentedQueueView(guest));
  QueuedRecordState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(kLightVector, std::as_writable_bytes(std::span(state.light)));
  hud_anchor::QueueAnchors anchors;
  queue.begin();
  for (std::uint32_t input = kSpriteQueue;; input += 4) {
    const std::uint32_t record = guest.mem_r32(input);
    if (record == 0) {
      break;
    }
    const bool screenSpace =
        (guest.mem_r16(record + moby::kCullDistance) & moby::kScreenSpace) != 0;
    state.record = record;
    state.hudShift =
        screenSpace ? spyro::ui_anchor::correction(guest,
                                                   {.name = "spyro1-hud-record", .index = record},
                                                   anchors.next(record))
                    : 0;
    guest.readBytes(record, std::as_writable_bytes(std::span(state.bytes)));
    const auto scope = guest.emission.instance(record);
    reads->clear();
    if (const auto chain = queue.draw(record, state.hudShift)) {
      queue.link(*chain);
    }
    memory::saveDraw(guest, state, reads->words());
  }
  queue.end();
}

} // namespace

void SpriteQueueProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  const SavedRecord before = readRecord(states.from);
  const SavedRecord after = readRecord(states.to);
  const QueuedRecordState &state = after.header;
  const spyro::guest_terrain::InBetweenCamera camera = spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, state.camera, states.t);
  InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.provide(state.record, recordBetween(before.header.bytes, state.bytes, states.t));
  between.mapCamera(camera);
  between.host().provide(kLightVector, std::as_bytes(std::span(state.light)));
  between.provideWord(kShadowListEnd, kShadowList);

  SpriteQueue queue(core_, between.memory(), presentedQueueView(core_), &sink);
  queue.begin();
  const std::optional<QueuedChain> chain = queue.draw(state.record, state.hudShift);
  if (!chain) {
    return;
  }
  const psx::present::OtSlot slot =
      chain->overlay ? psx::present::OtSlot{spyro::depth_bins::kOverlayTable, 0}
                     : psx::present::OtSlot{spyro::depth_bins::kTable, chain->depthBin};
  psx::present::DrawModeTracker drawMode;
  if (chain->drawMode != 0) {
    const std::array<std::uint32_t, 1> mode = {chain->modeCommand()};
    drawMode.take(mode);
  }
  between.emitChain(chain->first, slot, drawMode, sink);
}

void registerSpriteQueueOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 SpriteQueueProducer::kEntry,
                                 "spyro1-sprite-queue",
                                 drawQueue,
                                 std::make_unique<SpriteQueueProducer>(core));
}

} // namespace spyro1::actors
