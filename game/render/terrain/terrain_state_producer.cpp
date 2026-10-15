#include "terrain_state_producer.h"

#include "gp0_primitive_decode.h"
#include "guest_depth_bins.h"
#include "lucent/log.h"
#include "spyro_session.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace spyro::guest_terrain {
namespace {

struct SavedField {
  FieldState header;
  std::vector<LevelWord> readWords;
};

SavedField readField(std::span<const std::byte> bytes) {
  if (bytes.size() < sizeof(FieldState) ||
      (bytes.size() - sizeof(FieldState)) % sizeof(LevelWord) != 0) {
    lucent::error("terrstate", "a saved terrain field of {} bytes is not one", bytes.size());
    std::abort();
  }
  SavedField field;
  std::memcpy(&field.header, bytes.data(), sizeof(FieldState));
  field.readWords.resize((bytes.size() - sizeof(FieldState)) / sizeof(LevelWord));
  std::memcpy(field.readWords.data(),
              bytes.data() + sizeof(FieldState),
              field.readWords.size() * sizeof(LevelWord));
  return field;
}

} // namespace

FieldState fieldHeader(std::span<const std::byte> state) {
  return readField(state).header;
}

void drawField(Core &core, const Facts &facts, const guest_render_globals::Globals &globals) {
  FieldState state;
  state.camera = readCamera(core, globals);
  const std::vector<LevelWord> read = draw(core, facts, globals);
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
  std::vector<std::byte> bytes(sizeof(FieldState) + read.size() * sizeof(LevelWord));
  std::memcpy(bytes.data(), &state, sizeof(FieldState));
  std::memcpy(bytes.data() + sizeof(FieldState), read.data(), read.size() * sizeof(LevelWord));
  core.frameStates.save(core.emission.current(), std::span<const std::byte>(bytes));
}

void TerrainStateProducer::render(const psx::present::StateInterval &states,
                                  psx::present::PrimitiveSink &sink) const {
  const SavedField before = readField(states.from);
  const SavedField after = readField(states.to);
  const InBetweenCamera camera = cameraBetween(
      core_, globals_, before.header.camera, after.header.camera, static_cast<double>(states.t));
  const std::vector<std::uint8_t> visible =
      unionVisibility(std::span(before.header.visible, before.header.sectorCount),
                      std::span(after.header.visible, after.header.sectorCount));
  Rebuild rebuild(core_,
                  facts_,
                  globals_,
                  camera,
                  FieldInputs{.visibleSectors = visible,
                              .readWords = after.readWords,
                              .packetBudget = after.header.packetBudget});
  rebuild.forEachPacket(
      [&sink](std::uint32_t bin, std::uint32_t packet, std::span<const std::uint32_t> words) {
        const auto primitive = psx::gpu::decodePacketPrimitive(words);
        if (!primitive) {
          lucent::error("terrstate",
                        "the {}-word packet at 0x{:08X} in bin {} is not a polygon",
                        words.size(),
                        packet,
                        bin);
          std::abort();
        }
        sink.emit(psx::present::OtSlot{depth_bins::kTable, bin}, *primitive);
      });
}

} // namespace spyro::guest_terrain
