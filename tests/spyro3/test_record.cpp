// Spyro 3 on the Record path: the render path it selects, the words its cuts read, which flatten
// calls assign depth bins, and the terrain producer composed between two fields of a synthetic
// level.
#include "core.h"
#include "core/frame_cut.h"
#include "game.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro3/core/boot_facts.h"
#include "spyro3/core/runtime.h"
#include "spyro3/render/render_facts.h"
#include "terrain_record_scene.h"
#include "testutil.h"

#include <cstdint>
#include <memory>

namespace {

using psx::present::OtSlot;
using spyro::test::TerrainScene;

const spyro::test::TerrainTitle kSpyro3{
    .facts = spyro3::kTerrainFacts,
    .globals = spyro3::kRenderGlobals,
    .flatten = spyro3::kFlatten,
    .registerRenderOverrides = spyro::registerEngineRenderOverrides<spyro3::kRenderFacts>,
    .imageStart = 0x1E000u,
    .imageEnd = 0x50000u,
};

// The draw's own flatten call (8001EB74), and the second-pass flattens of states 4 and 0xD.
constexpr std::uint32_t kDrawFlatten = 0x8001EB74u;
constexpr std::uint32_t kStateFourSecondFlatten = 0x8001EA38u;
constexpr std::uint32_t kStateDSecondFlatten = 0x8001EB44u;

void test_spyro3_interpolates() {
  const spyro3::Runtime spyro3;
  CHECK(spyro3.interpolates());
}

void test_a_fresh_runtime_seals_a_cut_first() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  const spyro3::Runtime spyro3;
  CHECK(spyro3.sealedFrameIsCut(game->core));
}

void drawThenTail(spyro::FrameCut &cut, Core &core) {
  cut.onFrameDrawn(core);
  cut.onFrameTail(core);
}

// The cut reads this image's game state, level and camera machine, not Spyro 2's.
void test_the_cut_reads_spyro3s_scene_and_camera() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  Core &core = game->core;
  const spyro::FrameCutFacts &facts = spyro3::kBootPrefixFacts.cut;
  spyro::FrameCut cut(facts);
  core.mem_w32(facts.gameState, 0);
  core.mem_w32(facts.levelId, 10);
  drawThenTail(cut, core);
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
  core.mem_w32(facts.levelId, 11);
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  core.mem_w32(facts.gameState, 5);
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  CHECK(facts.camera.has_value());
  if (!facts.camera) {
    return;
  }
  core.mem_w32(facts.camera->mode, 0xB);
  core.mem_w32(facts.camera->subState, 0);
  drawThenTail(cut, core);
  core.mem_w32(facts.camera->subState, 1); // FUN_800144B4 placed the camera at the respawn
  drawThenTail(cut, core);
  CHECK(cut.isCut());
  drawThenTail(cut, core);
  CHECK(!cut.isCut());
}

// One packet linked into bin 7, then the flatten reached from `callSite`.
std::optional<OtSlot> binAfterFlattenFrom(std::uint32_t callSite) {
  TerrainScene scene(kSpyro3);
  Core &core = scene.core();
  constexpr std::uint32_t kPacket = 0x80130000u;
  constexpr std::uint32_t kBin = 7;
  constexpr std::uint32_t kBinPair = 8;
  core.mem_w32(kPacket, 0x01000000u);
  core.mem_w32(spyro::test::kTable + kBin * kBinPair, kPacket);
  core.mem_w32(spyro::test::kTable + kBin * kBinPair + 4, kPacket);
  scene.flattenFrom(callSite);
  return core.otTables.assigned(kPacket);
}

// The draw's flatten assigns every packet its bin; the second pass of states 4 and 0xD is chained
// after the first and assigns none.
void test_only_the_first_pass_flatten_assigns_bins() {
  CHECK(binAfterFlattenFrom(kDrawFlatten) == (OtSlot{spyro::depth_bins::kTable, 7}));
  CHECK(!binAfterFlattenFrom(kStateFourSecondFlatten).has_value());
  CHECK(!binAfterFlattenFrom(kStateDSecondFlatten).has_value());
}

// The drawer 0x80022378 is a producer through the runtime: t = 1 is the field as drawn and t = 0.5
// is the field drawn at the halfway camera, across the two packet pools.
void test_the_composer_draws_the_terrain_between_two_fields() {
  TerrainScene scene(kSpyro3);
  Core &core = scene.core();
  CHECK(core.game->runtime.stateProducer(core, spyro3::kTerrainFacts.entry) != nullptr);
  const spyro::test::ComposeCheck check = spyro::test::composeAcrossTwoFields(scene);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
  CHECK(check.keepsItsLevel);
}

} // namespace

int main() {
  RUN(spyro3_interpolates);
  RUN(a_fresh_runtime_seals_a_cut_first);
  RUN(the_cut_reads_spyro3s_scene_and_camera);
  RUN(only_the_first_pass_flatten_assigns_bins);
  RUN(the_composer_draws_the_terrain_between_two_fields);
  return psx::test::summary();
}
