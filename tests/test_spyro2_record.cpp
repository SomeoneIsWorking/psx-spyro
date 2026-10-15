// Spyro 2 on the Record path: the render path it selects, the cuts it declares, the depth bin each
// packet is assigned, and the terrain producer's render against the real draw over a synthetic
// level.
#include "core.h"
#include "frame_cut.h"
#include "game.h"
#include "guest_depth_bins.h"
#include "spyro2_boot_facts.h"
#include "spyro2_render_facts.h"
#include "spyro2_runtime.h"
#include "terrain_record_scene.h"
#include "testutil.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {

namespace terrain = spyro::guest_terrain;
using psx::present::OtSlot;
using spyro::test::Collect;
using spyro::test::Emitted;
using spyro::test::kRestCamera;
using spyro::test::kTable;
using spyro::test::TerrainScene;

constexpr std::uint32_t kTerrainEntry = spyro2::kTerrainFacts.entry;

const spyro::test::TerrainTitle kSpyro2{
    .facts = spyro2::kTerrainFacts,
    .globals = spyro2::kRenderGlobals,
    .flatten = spyro2::kFlatten,
    .registerRenderOverrides = spyro::registerEngineRenderOverrides<spyro2::kRenderFacts>,
    .imageStart = 0x1B000u,
    .imageEnd = 0x49000u,
};

void test_spyro2_replays_its_record_and_interpolates_it() {
  const spyro2::Spyro2Runtime spyro2;
  const RenderCapabilities caps = spyro2.renderCapabilities();
  CHECK(caps.defaultPath == RenderPath::Record);
  CHECK(spyro2.interpolates());
  CHECK(!caps.nativeRenderPath);
}

// One step of the driver: a draw returns (walking its table), then the step's tail.
void drawThenTail(spyro::FrameCut &cut, Core &core) {
  cut.onFrameDrawn(core);
  cut.onFrameTail(core);
}

void setScene(Core &core, std::uint32_t state, std::uint32_t level) {
  core.mem_w32(spyro2::kBootPrefixFacts.cut.gameState, state);
  core.mem_w32(spyro2::kBootPrefixFacts.cut.levelId, level);
}

void test_a_cut_is_a_new_game_state_or_level() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  Core &core = game->core;
  spyro::FrameCut cut(spyro2::kBootPrefixFacts.cut);
  CHECK(cut.isCut()); // nothing sealed yet
  setScene(core, 0, 11);
  drawThenTail(cut, core);
  CHECK(cut.isCut()); // the first record has no predecessor
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
  setScene(core, 1, 11);
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
  setScene(core, 1, 12);
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  // Anything else the guest changes, the camera included, is not a scene change.
  core.mem_w32(0x80067EACu, 0x1234u);
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
}

struct Camera {
  std::uint32_t mode = 0;
  std::uint32_t subState = 0;
};

// A record is walked in the camera state `after`, the one before it in `before`.
struct CameraStep {
  Camera before;
  Camera after;
};

void setCamera(Core &core, Camera camera) {
  const std::optional<spyro::CameraMachineWords> &words = spyro2::kBootPrefixFacts.cut.camera;
  CHECK(words.has_value());
  if (!words) {
    return;
  }
  core.mem_w32(words->mode, camera.mode);
  core.mem_w32(words->subState, camera.subState);
}

bool placementCuts(const CameraStep &step) {
  auto game = std::make_unique<spyro::test::SessionGame>();
  Core &core = game->core;
  spyro::FrameCut cut(spyro2::kBootPrefixFacts.cut);
  setScene(core, 0, 11);
  setCamera(core, step.before);
  drawThenTail(cut, core);
  setCamera(core, step.after);
  drawThenTail(cut, core);
  return cut.isCut();
}

// The camera controller places the camera at three steps of its mode machine (FUN_8001FA58); every
// other camera change is the follow camera moving.
void test_a_cut_is_a_camera_placement_inside_one_scene() {
  CHECK(placementCuts({{9, 0}, {9, 1}})); // saved camera: the fade ended, the camera is placed
  CHECK(placementCuts({{9, 0x80}, {9, 0x81}}));   // the same with the sub-state's bit 7 set
  CHECK(placementCuts({{9, 0}, {9, 2}}));         // two updates between the draws
  CHECK(placementCuts({{0xB, 0}, {0xB, 1}}));     // respawn
  CHECK(placementCuts({{10, 0}, {10, 1}}));       // the scripted camera begins
  CHECK(placementCuts({{10, 0x80}, {10, 0x81}})); // the scripted camera ends

  CHECK(!placementCuts({{9, 0}, {9, 0}}));    // still fading out
  CHECK(!placementCuts({{9, 0}, {9, 0x80}})); // a flag, not a placement
  CHECK(!placementCuts({{9, 1}, {9, 2}}));    // after the placement
  CHECK(!placementCuts({{0xB, 1}, {0xB, 1}}));
  CHECK(!placementCuts({{10, 1}, {10, 2}}));    // keys of the script are the camera moving
  CHECK(!placementCuts({{10, 2}, {10, 0x80}})); // the script's end fade starts
  CHECK(!placementCuts({{10, 0x81}, {10, 0x82}}));
  CHECK(!placementCuts({{10, 3}, {10, 2}})); // the blend into the script
  CHECK(!placementCuts({{6, 0}, {0, 1}}));   // follow modes keep the camera where it is
  CHECK(!placementCuts({{0, 1}, {1, 0}}));
  CHECK(!placementCuts({{0xB, 0}, {9, 0}})); // a mode change is not a step of the machine
}

// The scene is the one the walked table was drawn in, not the state at the tail: the update that
// runs after the draw returns may already have moved on.
void test_the_scene_is_sampled_when_the_table_is_walked() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  Core &core = game->core;
  spyro::FrameCut cut(spyro2::kBootPrefixFacts.cut);
  setScene(core, 0, 11);
  drawThenTail(cut, core);
  cut.onFrameDrawn(core);
  setScene(core, 5, 11); // the next update enters a load before the tail
  cut.onFrameTail(core);
  CHECK(!cut.isCut());
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  // A step whose draw did not return walked no table: not a scene change, and the walk after it
  // compares with the last walk.
  cut.onFrameTail(core);
  CHECK(!cut.isCut());
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
  setScene(core, 0, 12);
  cut.onFrameTail(core);
  CHECK(!cut.isCut());
  drawThenTail(cut, core);
  CHECK(cut.isCut());
}

void test_a_fresh_runtime_seals_a_cut_first() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  const spyro2::Spyro2Runtime spyro2;
  CHECK(spyro2.sealedFrameIsCut(game->core));
}

// The flatten override hands every packet of every bin its bin, chains and singletons alike, and
// still runs the original body.
void test_the_flatten_override_assigns_each_packet_its_bin() {
  TerrainScene scene(kSpyro2);
  Core &core = scene.core();
  constexpr std::uint32_t kBinPair = 8;
  constexpr std::uint32_t kPackets[4] = {0x80130000u, 0x80130040u, 0x80130080u, 0x801300C0u};
  const auto link = [&core](std::uint32_t from, std::uint32_t to) {
    core.mem_w32(from, 0x01000000u | (to == 0 ? 0u : psx::gpu::mainRamOffsetOf(to)));
  };
  link(kPackets[0], kPackets[1]); // bin 5: first, middle, head
  link(kPackets[1], kPackets[2]);
  link(kPackets[2], 0);
  link(kPackets[3], 0); // bin 200: one packet
  core.mem_w32(kTable + 5 * kBinPair, kPackets[2]);
  core.mem_w32(kTable + 5 * kBinPair + 4, kPackets[0]);
  core.mem_w32(kTable + 200 * kBinPair, kPackets[3]);
  core.mem_w32(kTable + 200 * kBinPair + 4, kPackets[3]);
  core.mem_w32(spyro2::kRenderGlobals.orderingTable, kTable);
  core.mem_w32(spyro2::kRenderGlobals.orderingTableMark, kTable + terrain::kOrderingTableBytes);

  scene.flatten();

  const OtSlot five{spyro::depth_bins::kTable, 5};
  CHECK(core.otTables.assigned(kPackets[0]) == five);
  CHECK(core.otTables.assigned(kPackets[1]) == five);
  CHECK(core.otTables.assigned(kPackets[2]) == five);
  CHECK(core.otTables.assigned(kPackets[3]) == (OtSlot{spyro::depth_bins::kTable, 200}));
  CHECK(core.otTables.slotOf(kTable + 200 * kBinPair) == (OtSlot{spyro::depth_bins::kTable, 200}));
}

// At t = 1 the render emits the packets the real draw linked: the same decoded primitives into the
// same buckets, deepest bin first; and the real draw's packets all belong to one object whose saved
// state the producer reads.
void test_the_render_at_one_emits_what_the_real_draw_linked() {
  TerrainScene scene(kSpyro2);
  Core &core = scene.core();
  scene.drawTerrain();
  scene.flatten();

  const spyro::test::Linked real = scene.linkedPackets();
  CHECK(real.packets.size() >= 2); // the quad and the triangle
  CHECK_EQ(real.undecoded, 0u);
  for (std::size_t i = 0; i < real.packets.size(); ++i) {
    CHECK(real.assigned[i] == real.packets[i].slot);
  }
  const std::uint32_t first = scene.firstLinkedPacket();
  const std::optional<psx::present::RecordKey> key = core.emission.keyFor(first);
  CHECK(key.has_value());
  CHECK(key && key->producer == kTerrainEntry && key->object == 0 && key->element == 0);

  const auto state = scene.savedState(real.packets.front().primitive, first);
  CHECK(state.has_value());
  const psx::present::StateProducer *producer =
      core.game->runtime.stateProducer(core, kTerrainEntry);
  CHECK(producer != nullptr);
  if (!state || producer == nullptr) {
    return;
  }
  const terrain::FieldState field = terrain::fieldHeader(*state);
  CHECK_EQ(field.sectorCount, 1u);
  CHECK_EQ(static_cast<unsigned>(field.visible[0]), 1u);
  CHECK_EQ(field.camera.position[1], kRestCamera.position[1]);

  Collect sink;
  producer->render({.from = *state, .to = *state, .t = 1.0f}, sink);
  CHECK(sink.emitted == real.packets);
}

// A saved field keeps the level words the drawer read and none of the ranges the traversal owns:
// an in-between maps those empty, so a saved copy would only cost space.
void test_a_saved_field_keeps_only_level_words() {
  TerrainScene scene(kSpyro2);
  scene.drawTerrain();
  scene.flatten();
  const spyro::test::Linked real = scene.linkedPackets();
  CHECK(!real.packets.empty());
  if (real.packets.empty()) {
    return;
  }
  const auto state = scene.savedState(real.packets.front().primitive, scene.firstLinkedPacket());
  CHECK(state.has_value());
  if (!state) {
    return;
  }
  const std::size_t count =
      (state->size() - sizeof(terrain::FieldState)) / sizeof(spyro::guest_memory::LevelWord);
  std::vector<spyro::guest_memory::LevelWord> words(count);
  std::memcpy(words.data(), state->data() + sizeof(terrain::FieldState), count * sizeof(words[0]));
  const spyro::guest_memory::GuestRange owned[] = {
      {terrain::kScratchpad, terrain::kScratchpadBytes},
      {spyro::test::kScratchEnd - spyro2::kTerrainFacts.scratchListsBelowEnd,
       spyro2::kTerrainFacts.scratchListsBelowEnd},
      {kTable, terrain::kOrderingTableBytes},
  };
  bool levelRead = false;
  for (const spyro::guest_memory::LevelWord &word : words) {
    for (const spyro::guest_memory::GuestRange &range : owned) {
      CHECK(!range.contains(word.address));
    }
    levelRead = levelRead ||
                (word.address >= spyro::test::kSector && word.address < spyro::test::kSectorTable);
  }
  CHECK(levelRead);
}

// Halfway between two cameras the render differs from t = 1, still draws, and holds only finite,
// on-grid values in bins the table has.
void test_the_render_between_two_cameras_differs_and_is_finite() {
  TerrainScene scene(kSpyro2);
  Core &core = scene.core();
  scene.drawTerrain();
  scene.flatten();
  const spyro::test::Linked real = scene.linkedPackets();
  CHECK(!real.packets.empty());
  const std::uint32_t first = scene.firstLinkedPacket();
  const auto after = scene.savedState(real.packets.front().primitive, first);
  const psx::present::StateProducer *producer =
      core.game->runtime.stateProducer(core, kTerrainEntry);
  CHECK(after.has_value());
  CHECK(producer != nullptr);
  if (!after || producer == nullptr) {
    return;
  }

  terrain::FieldState before = terrain::fieldHeader(*after);
  before.camera.angles[1] = 0x20;
  before.camera.position[0] += 0x40;
  std::vector<std::byte> beforeBytes(after->begin(), after->end());
  std::memcpy(beforeBytes.data(), &before, sizeof before);
  Collect half;
  Collect one;
  producer->render({.from = beforeBytes, .to = *after, .t = 0.5f}, half);
  producer->render({.from = beforeBytes, .to = *after, .t = 1.0f}, one);

  CHECK(!half.emitted.empty());
  CHECK(half.emitted != one.emitted);
  CHECK(one.emitted == real.packets);
  for (const Emitted &emitted : half.emitted) {
    CHECK(emitted.slot.table == spyro::depth_bins::kTable);
    CHECK(emitted.slot.index < spyro::depth_bins::kBins);
    for (int i = 0; i < emitted.primitive.vertexCount; ++i) {
      const psx::present::RecordVertex &vertex =
          emitted.primitive.vertices[static_cast<std::size_t>(i)];
      CHECK(std::isfinite(vertex.subX) && std::isfinite(vertex.subY));
      CHECK(std::abs(vertex.x) < 4096 && std::abs(vertex.y) < 4096);
    }
  }
}

// Two fields drawn into the two packet pools compose: t = 1 is the field as drawn and t = 0.5 is
// the field drawn at the halfway camera.
void test_the_composer_draws_the_terrain_between_two_fields() {
  TerrainScene scene(kSpyro2);
  const spyro::test::ComposeCheck check = spyro::test::composeAcrossTwoFields(scene);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
  CHECK(check.keepsItsLevel);
}

} // namespace

int main() {
  RUN(spyro2_replays_its_record_and_interpolates_it);
  RUN(a_cut_is_a_new_game_state_or_level);
  RUN(a_cut_is_a_camera_placement_inside_one_scene);
  RUN(the_scene_is_sampled_when_the_table_is_walked);
  RUN(a_fresh_runtime_seals_a_cut_first);
  RUN(the_flatten_override_assigns_each_packet_its_bin);
  RUN(the_render_at_one_emits_what_the_real_draw_linked);
  RUN(a_saved_field_keeps_only_level_words);
  RUN(the_render_between_two_cameras_differs_and_is_finite);
  RUN(the_composer_draws_the_terrain_between_two_fields);
  return pt_summary();
}
