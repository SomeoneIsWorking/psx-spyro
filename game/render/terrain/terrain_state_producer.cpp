#include "render/terrain/terrain_state_producer.h"

#include "core/session.h"
#include "lucent/log.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>
#include <vector>

namespace spyro::guest_terrain {
namespace {

using SavedField = guest_memory::SavedDraw<FieldState>;

SavedField readField(std::span<const std::byte> bytes) {
  return guest_memory::unpackDraw<FieldState>(bytes, "terrain field");
}

} // namespace

FieldState fieldHeader(std::span<const std::byte> state) {
  return readField(state).header;
}

void drawField(Core &core, const TerrainImage &image) {
  FieldState state;
  state.camera = readCamera(core, image.globals());
  state.argument = core.r[4];
  const psx::present::Kept<psx::present::LevelReads>::Use read(spyro::session(core).drawReads);
  draw(core, image, *read);
  const std::vector<std::uint8_t> &visible = spyro::session(core).terrainVisibility;
  if (visible.size() > kScratchpadBytes) {
    lucent::error("terrstate",
                  "refusing to save a terrain field: the level declares {} sectors and the state "
                  "holds {}",
                  visible.size(),
                  kScratchpadBytes);
    std::abort();
  }
  state.packetBudget = spyro::session(core).terrainPacketBudget;
  state.sectorCount = static_cast<std::uint32_t>(visible.size());
  std::copy(visible.begin(), visible.end(), state.visible);
  // In address order, so an in-between can tell a saved word from one it would read live.
  const psx::present::Kept<std::vector<psx::present::LevelWord>>::Use sorted(
      spyro::session(core).terrainSavedReads);
  sorted->assign(read->words().begin(), read->words().end());
  std::ranges::sort(*sorted, {}, &psx::present::LevelWord::address);
  guest_memory::saveDraw(core, state, std::span<const psx::present::LevelWord>(*sorted));
}

psx::present::StateProducer::Composition
TerrainStateProducer::composition(const psx::present::StateInterval &states) const {
  rebuild_.reset();
  const SavedField before = readField(states.from);
  const SavedField after = readField(states.to);
  const InBetweenCamera camera =
      cameraBetween(core_, image_, before.header.camera, after.header.camera, states.t);
  std::array<std::uint8_t, kScratchpadBytes> sectors{};
  const std::span<const std::uint8_t> visible =
      unionVisibility(std::span(before.header.visible, before.header.sectorCount),
                      std::span(after.header.visible, after.header.sectorCount),
                      sectors);
  rebuild_.emplace(core_,
                   image_,
                   camera,
                   FieldInputs{.visibleSectors = visible,
                               .earlierReads = before.reads,
                               .laterReads = after.reads,
                               .argument = after.header.argument,
                               .packetBudget = after.header.packetBudget});
  if (!rebuild_->covered()) {
    rebuild_.reset();
    return Composition::Refused;
  }
  rebuiltFor_ = states;
  return Composition::Render;
}

void TerrainStateProducer::render(const psx::present::StateInterval &states,
                                  psx::present::PrimitiveSink &sink) const {
  if (!rebuild_ || states.from.data() != rebuiltFor_.from.data() ||
      states.to.data() != rebuiltFor_.to.data() || states.t != rebuiltFor_.t) {
    lucent::error(
        "terrstate", "a terrain render at t={} that its composition did not rebuild", states.t);
    std::abort();
  }
  rebuild_->emit(sink);
  rebuild_.reset();
}

} // namespace spyro::guest_terrain
