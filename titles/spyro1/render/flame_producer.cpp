#include "spyro1/render/flame_producer.h"

#include "blend.h"
#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/flame.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <vector>

namespace spyro1::flame {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;
using Block = std::array<std::uint8_t, flame_block::kBytes>;

constexpr std::uint32_t at(std::uint32_t address) {
  return address - flame_block::kBlock;
}

void drawFlame(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<GuestSpan, 3> working = actors::binnedWorkingRanges(guest);
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  FlameState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(flame_block::kBlock, std::as_writable_bytes(std::span(state.block)));
  const auto scope = guest.emission.instance(flame_block::kBlock);
  FlameTrails flame(guest, memory);
  flame.draw();
  flame.end();
  memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
}

// The block at `t`: the anchor and rotation moved, a part that grew between the two saves part of
// the way on, everything else `to`'s.
Block blockBetween(const Block &from, const Block &to, float t) {
  Block shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    spyro::record_blend::wordBetween(shown, from, to, at(flame_block::kPosition) + 4 * axis, t);
  }
  for (std::uint32_t word = at(flame_block::kRotation);
       word < at(flame_block::kRotation) + flame_block::kRotationBytes;
       word += 4) {
    spyro::record_blend::store(
        shown,
        word,
        psx::present::lerpHalves(spyro::record_blend::load<std::uint32_t>(from, word),
                                 spyro::record_blend::load<std::uint32_t>(to, word),
                                 t));
  }
  for (std::uint32_t part = 0; part < flame_block::kParts; ++part) {
    const std::uint32_t length = at(flame_block::kLengths) + part;
    const std::uint32_t limit = at(flame_block::kLimits) + part;
    const auto before = static_cast<std::int8_t>(from[length]);
    const auto after = static_cast<std::int8_t>(to[length]);
    if (before > 0 && after > before && from[limit] == to[limit]) {
      shown[length] = spyro::record_blend::byteBetween(from[length], to[length], t);
    }
  }
  return shown;
}

} // namespace

void FlameProducer::render(const psx::present::StateInterval &states,
                           psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<FlameState>(states.from, "flame draw");
  const auto after = memory::unpackDraw<FlameState>(states.to, "flame draw");
  actors::InBetween between(core_, after.reads);
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t));
  between.mapTable();
  between.provide(flame_block::kBlock,
                  blockBetween(before.header.block, after.header.block, states.t));
  FlameTrails(core_, between.memory(), &sink).draw();
  between.emitBins(sink);
}

void registerFlameOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 FlameProducer::kEntry,
                                 "spyro1-flame",
                                 drawFlame,
                                 std::make_unique<FlameProducer>(core));
}

} // namespace spyro1::flame
