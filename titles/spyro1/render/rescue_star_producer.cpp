#include "spyro1/render/rescue_star_producer.h"

#include "blend.h"
#include "core.h"
#include "core/session.h"
#include "gte_control.h"
#include "render/field/guest_render_globals.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <cstring>
#include <memory>

namespace spyro1::rescue_star {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;
using Block = std::array<std::uint8_t, kBytes>;

constexpr std::uint32_t at(std::uint32_t address) {
  return address - kBlock;
}

void drawStar(Core *core) {
  Core &guest = *core;
  const std::array<GuestSpan, 5> working =
      actors::binnedWorkingRanges(guest,
                                  GuestSpan{kStaging, kStagingBytes},
                                  GuestSpan{guest.mem_r32(kRenderGlobals.overlayPair), 8});
  const psx::present::Kept<psx::present::LevelReads>::Use reads(spyro::session(guest).drawReads);
  const psx::present::EmitMemory memory(guest, *reads, working);
  StarState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(kBlock, std::as_writable_bytes(std::span(state.block)));
  const auto scope = guest.emission.instance(kBlock);
  RescueStar star(guest, memory);
  star.draw();
  star.end();
  memory::saveDraw(guest, state, reads->words());
}

// The block at `t`: centre, rotation, scale and grey moved.
Block blockBetween(const Block &from, const Block &to, float t) {
  Block shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    spyro::record_blend::wordBetween(shown, from, to, at(kPosition) + 4 * axis, t);
  }
  spyro::record_blend::wordBetween(shown, from, to, at(kScale), t);
  shown[at(kColour)] = spyro::record_blend::byteBetween(from[at(kColour)], to[at(kColour)], t);
  const auto rotationOf = [](const Block &block) {
    psx::gte::RotationWords words{};
    std::memcpy(words.data(), block.data() + at(kRotation), sizeof(words));
    return words;
  };
  const psx::gte::RotationWords rotation =
      psx::present::rotationBetween(rotationOf(from), rotationOf(to), t);
  std::memcpy(shown.data() + at(kRotation), rotation.data(), sizeof(rotation));
  return shown;
}

} // namespace

void RescueStarProducer::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<StarState>(states.from, "rescue star draw");
  const auto after = memory::unpackDraw<StarState>(states.to, "rescue star draw");
  actors::InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t));
  between.provide(kBlock, blockBetween(before.header.block, after.header.block, states.t));
  const std::uint32_t pair = between.memory().mem_r32(kRenderGlobals.overlayPair);
  between.provideWord(pair, 0);
  between.provideWord(pair + 4, 0);
  const std::uint32_t first = RescueStar(core_, between.memory(), &sink).draw();
  psx::present::DrawModeTracker drawMode;
  between.emitChain(first, spyro::depth_bins::kOverlaySlot, drawMode, sink);
}

void registerRescueStarOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 RescueStarProducer::kEntry,
                                 "spyro1-rescue-star",
                                 drawStar,
                                 std::make_unique<RescueStarProducer>(core));
}

} // namespace spyro1::rescue_star
