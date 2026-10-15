#include "spyro1/render/sky_producer.h"

#include "core.h"
#include "core/session.h"
#include "gp0_primitive_decode.h"
#include "gte_control.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "render/field/guest_packet_chain.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <array>
#include <cstdlib>
#include <memory>
#include <vector>

namespace spyro1::sky {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

// The camera's view rotation, which the classification pass reads (terrain::kSharedFacts).
constexpr std::uint32_t kViewRotation = terrain::kSharedFacts.classify.viewRotation;

// The rotations the call's $a1 and $a2 name.
Rotations readRotations(Core &core) {
  const std::uint32_t view = core.r[5];
  const std::uint32_t projection = core.r[6];
  Rotations rotations;
  for (std::uint32_t i = 0; i < psx::gte::kRotationWords; ++i) {
    rotations.view[i] = core.mem_r32(view + i * 4);
    rotations.projection[i] = core.mem_r32(projection + i * 4);
  }
  return rotations;
}

void drawSky(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::uint32_t view = guest.r[5];
  const std::uint32_t projection = guest.r[6];
  const std::uint32_t cursor = guest.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t poolEnd = guest.mem_r32(terrain::kPacketPoolEnd);
  const std::array<memory::GuestRange, 7> working = {{
      {kOutcodes, kOutcodesBytes},
      {kSectorList, kSectorListBytes},
      {cursor, cursor < poolEnd ? poolEnd - cursor : 0u},
      {guest.mem_r32(kRenderGlobals.orderingTable), terrain::kOrderingTableBytes},
      {kRenderGlobals.primitiveCursor, 4},
      {terrain::kPacketPoolEnd, 4},
      {kPoolExhausted, 4},
  }};
  memory::GuestMemory reads(guest, working);
  const SkyState state{
      .camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals),
      .rotations = readRotations(guest),
      .group = static_cast<std::int32_t>(guest.r[4]),
      .cameraRotations =
          view == kViewRotation && projection == kRenderGlobals.cameraRotation ? 1u : 0u,
  };
  Cyclorama sky(guest, reads, spyro::presentedBounds(guest, spyro1::terrain::kSharedFacts));
  sky.draw(state.group, state.rotations);
  sky.end();
  const std::vector<std::byte> saved = memory::packDraw(state, reads.firstReads());
  guest.frameStates.save(guest.emission.current(), std::span<const std::byte>(saved));
}

} // namespace

Rotations rotationsBetween(Core &core, const SkyState &from, const SkyState &to, float t) {
  if (from.cameraRotations == 0 || to.cameraRotations == 0) {
    return to.rotations;
  }
  const spyro::guest_terrain::InBetweenCamera camera = spyro::guest_terrain::cameraBetween(
      core, terrain::kTerrainImage, from.camera, to.camera, static_cast<double>(t));
  Rotations rotations;
  for (std::uint32_t i = 0; i < psx::gte::kRotationWords; ++i) {
    rotations.view[i] = camera.viewRotation[i];
    rotations.projection[i] = camera.rotation[i];
  }
  return rotations;
}

void SkyProducer::render(const psx::present::StateInterval &states,
                         psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<SkyState>(states.from, "sky draw");
  const auto after = memory::unpackDraw<SkyState>(states.to, "sky draw");
  const Rotations rotations = rotationsBetween(core_, before.header, after.header, states.t);
  const memory::PacketArena arena = terrain::kTerrainImage.windows(core_).inBetweenArena;
  memory::HostMemory host(core_);
  host.mapEmpty(kOutcodes, kOutcodesBytes);
  host.mapEmpty(kSectorList, kSectorListBytes);
  host.mapEmpty(arena.base, arena.bytes);
  host.overlay(after.reads);
  const std::uint32_t table = host.r32(kRenderGlobals.orderingTable);
  host.mapEmpty(table, terrain::kOrderingTableBytes);
  host.mapEmpty(kRenderGlobals.primitiveCursor, 4);
  host.w32(kRenderGlobals.primitiveCursor, arena.base);
  host.mapEmpty(terrain::kPacketPoolEnd, 4);
  host.w32(terrain::kPacketPoolEnd, arena.base + arena.bytes);
  host.mapEmpty(kPoolExhausted, 4);

  const psx::present::GteGuard gte(core_.gte());
  Cyclorama sky(core_, host, spyro::presentedBounds(core_, spyro1::terrain::kSharedFacts));
  const std::optional<SkyChain> chain = sky.draw(after.header.group, rotations);
  if (!chain) {
    return;
  }
  memory::PacketChains chains(host, arena);
  chains.walk(chain->first, [&](std::uint32_t packet, std::span<const std::uint32_t> words) {
    const auto primitive = psx::gpu::decodePacketPrimitive(words);
    if (!primitive) {
      lucent::error("sky", "the packet 0x{:08X} at 0x{:08X} is not a polygon", words[0], packet);
      std::abort();
    }
    sink.emit(psx::present::OtSlot{spyro::depth_bins::kTable, kSkyBin}, *primitive);
  });
}

void registerSkyOverrides(Core &core) {
  spyro::session(core).stateProducers.install(SkyProducer::kEntry,
                                              std::make_unique<SkyProducer>(core));
  psx::cpu::installNativeOverride(core,
                                  SkyProducer::kEntry,
                                  "spyro1-sky",
                                  drawSky,
                                  psx::present::Producer{psx::present::Arg::A0});
}

} // namespace spyro1::sky
