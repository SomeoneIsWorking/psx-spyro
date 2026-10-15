#include "spyro1_sprite_queue_producer.h"

#include "core.h"
#include "gp0_primitive_decode.h"
#include "gte_control.h"
#include "gte_registers.h"
#include "guest_depth_bins.h"
#include "guest_packet_chain.h"
#include "guest_render_globals.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "spyro1_actor_between.h"
#include "spyro1_actor_facts.h"
#include "spyro1_hud_anchor.h"
#include "spyro1_ordering_table.h"
#include "spyro1_sprite_queue.h"
#include "spyro1_terrain_image.h"
#include "spyro_session.h"

#include <cstdlib>
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
  const std::uint32_t cursor = guest.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = guest.mem_r32(terrain::kPacketPoolEnd);
  const std::array<memory::GuestRange, 7> working = {{
      {kVertexScreens, kScratchpadBytes},
      {kWorkBlock, kWorkBlockBytes},
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {guest.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {guest.mem_r32(OrderingTable::kOverlayPairPointer), 8},
      {kRenderGlobals.primitiveCursor, 4},
      {kShadowListEnd, 4},
  }};
  memory::GuestMemory reads(guest, working);
  SpriteQueue queue(guest, reads, presentedQueueView(guest));
  QueuedRecordState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  for (std::uint32_t i = 0; i < 3; ++i) {
    state.light[i] = guest.mem_r32(kLightVector + i * 4);
  }
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
    state.hudShift = screenSpace
                         ? hud_anchor::columns(guest,
                                               {.name = "spyro1-hud-record", .index = record},
                                               anchors.next(record))
                         : 0;
    for (std::uint32_t at = 0; at < state.bytes.size(); ++at) {
      state.bytes[at] = guest.mem_r8(record + at);
    }
    const auto scope = guest.emission.instance(record);
    reads.forgetReads();
    if (const auto chain = queue.draw(record, state.hudShift)) {
      queue.link(*chain);
    }
    const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
    guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
  }
  queue.end();
}

} // namespace

void SpriteQueueProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  const SavedRecord before = readRecord(states.from);
  const SavedRecord after = readRecord(states.to);
  const QueuedRecordState &state = after.header;
  const spyro::guest_terrain::InBetweenCamera camera =
      spyro::guest_terrain::cameraBetween(core_,
                                          terrain::kTerrainImage,
                                          before.header.camera,
                                          state.camera,
                                          static_cast<double>(states.t));
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  host.mapEmpty(kVertexScreens, kScratchpadBytes);
  host.mapEmpty(kWorkBlock, kWorkBlockBytes);
  host.mapEmpty(arena.base, arena.bytes);
  host.overlay(after.reads);
  const std::array<std::uint8_t, 0x58> record =
      recordBetween(before.header.bytes, state.bytes, states.t);
  host.mapEmpty(state.record, static_cast<std::uint32_t>(record.size()));
  for (std::uint32_t at = 0; at < record.size(); ++at) {
    host.w8(state.record + at, record[at]);
  }
  mapCamera(host, camera);
  host.mapEmpty(kLightVector, 12);
  for (std::uint32_t i = 0; i < 3; ++i) {
    host.w32(kLightVector + i * 4, state.light[i]);
  }
  host.mapEmpty(kRenderGlobals.primitiveCursor, 4);
  host.w32(kRenderGlobals.primitiveCursor, arena.base);
  host.mapEmpty(kShadowListEnd, 4);
  host.w32(kShadowListEnd, kShadowList);

  const psx::present::GteGuard gte;
  SpriteQueue queue(core_, host, presentedQueueView(core_));
  queue.begin();
  const std::optional<QueuedChain> chain = queue.draw(state.record, state.hudShift);
  if (!chain) {
    return;
  }
  const psx::present::OtSlot slot =
      chain->overlay ? psx::present::OtSlot{OrderingTable::kOverlayTable, 0}
                     : psx::present::OtSlot{spyro::depth_bins::kTable, chain->depthBin};
  memory::DrawMode drawMode;
  if (chain->drawMode != 0) {
    const std::array<std::uint32_t, 1> mode = {chain->modeCommand()};
    drawMode.take(mode);
  }
  memory::PacketChains chains(host, arena);
  chains.walk(chain->first, [&](std::uint32_t packet, std::span<const std::uint32_t> words) {
    auto primitive = psx::gpu::decodePacketPrimitive(words);
    if (!primitive) {
      lucent::error(
          "spritequeue", "the packet 0x{:08X} at 0x{:08X} is not a polygon", words[0], packet);
      std::abort();
    }
    drawMode.apply(*primitive);
    sink.emit(slot, *primitive);
  });
}

void registerSpriteQueueOverrides(Core &core) {
  spyro::session(core).stateProducers.install(SpriteQueueProducer::kEntry,
                                              std::make_unique<SpriteQueueProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  SpriteQueueProducer::kEntry,
                                  "spyro1-sprite-queue",
                                  drawQueue,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::actors
