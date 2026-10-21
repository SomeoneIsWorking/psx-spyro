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
  guest_memory::saveDraw(core, state, read->words());
}

void TerrainStateProducer::render(const psx::present::StateInterval &states,
                                  psx::present::PrimitiveSink &sink) const {
  const SavedField before = readField(states.from);
  const SavedField after = readField(states.to);
  const InBetweenCamera camera =
      cameraBetween(core_, image_, before.header.camera, after.header.camera, states.t);
  std::array<std::uint8_t, kScratchpadBytes> sectors{};
  const std::span<const std::uint8_t> visible =
      unionVisibility(std::span(before.header.visible, before.header.sectorCount),
                      std::span(after.header.visible, after.header.sectorCount),
                      sectors);
  Rebuild rebuild(core_,
                  image_,
                  camera,
                  FieldInputs{.visibleSectors = visible,
                              .readWords = after.reads,
                              .argument = after.header.argument,
                              .packetBudget = after.header.packetBudget},
                  sink);
  rebuild.emit(sink);
}

} // namespace spyro::guest_terrain
