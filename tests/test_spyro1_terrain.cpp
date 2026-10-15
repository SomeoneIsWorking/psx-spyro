// Spyro 1's terrain drawer 0x800258F0 through the shipping overrides: its camera builder 0x80033C50
// and its state producer, composed between two fields of a one-sector synthetic level.
#include "core.h"
#include "game.h"
#include "guest_camera_builder.h"
#include "record_wide_plan.h"
#include "spyro1_record_scene.h"
#include "spyro1_terrain_facts.h"
#include "spyro1_terrain_image.h"
#include "spyro_session.h"
#include "terrain_record_scene.h"
#include "testutil.h"

#include <lucent/log.h>

#include <cstdint>
#include <exception>

namespace {

namespace cam = spyro::guest_camera;
namespace terrain = spyro::guest_terrain;
namespace s1 = spyro1::terrain;
using spyro::test::DrawnFrame;
using spyro::test::kTable;

constexpr std::uint32_t kSector = 0x80010000u;
constexpr std::uint32_t kSectorTable = 0x80010400u;
constexpr std::uint32_t kTextures = 0x80011000u;

std::uint32_t vertexWord(std::uint32_t z, std::uint32_t x, std::uint32_t y) {
  return ((z / 4) << 21) | ((x / 4) << 10) | (y / 4);
}

std::uint32_t packedIndices(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
  return (a << 24) | (b << 16) | (c << 8) | d;
}

class Spyro1Scene {
public:
  Spyro1Scene() {
    plantLevel();
  }

  Core &core() {
    return field_.core();
  }

  [[nodiscard]] const psx::present::StateProducers &producers() {
    return field_.producers();
  }

  DrawnFrame drawFrame(const terrain::CameraState &camera, std::uint32_t field) {
    field_.beginField(camera, field);
    psx::cpu::callGuestNow(core(), "test", s1::kSharedFacts.entry, 0xFFFFFFFFu);
    return field_.endField();
  }

  // Moves the sector `units` (camera units / 16) sideways, sphere and vertices together.
  void shiftSector(std::uint32_t units) {
    core().mem_w32(kSector + 0x00, (0x300u << 16) | (0x80u + units));
    core().mem_w32(kSector + 0x08, (0xC00u << 14) | units);
  }

  void rewriteLevel() {
    core().mem_w32(kSector + 0x1C + 4 * 2, vertexWord(0x40, 0x240, 0x120));
  }

private:
  // One near sector 0xC00 deep: six vertices, four colours, a quad and a triangle, both two-sided
  // and never split. The far pass is skipped.
  void plantLevel() {
    core().mem_w32(s1::kFacts.lodTable + s1::lod::kSectors, kSectorTable);
    core().mem_w32(s1::kFacts.lodTable + s1::lod::kSectorCount, 1);
    core().mem_w32(s1::kFacts.lodTable + s1::lod::kDetailTextures, kTextures);
    core().mem_w32(s1::kFacts.lodTable + s1::lod::kNearDepth, 0x10000u);
    core().mem_w32(s1::kFacts.lodTable + s1::lod::kFarDepth, 0x1C000u);
    core().mem_w32(s1::kFacts.farPassSkip, 1);
    core().mem_w32(kSectorTable, kSector);
    core().mem_w32(kSector + 0x00, (0x300u << 16) | 0x80u); // centre x, y in camera units / 16
    core().mem_w32(kSector + 0x04, (0x40u << 16) | 0x80u);  // centre z, radius, no flags
    core().mem_w32(kSector + 0x08, 0xC00u << 14);
    core().mem_w32(kSector + 0x0C, 0x1800u); // the polygon flag bytes, inside the scratch block
    core().mem_w32(kSector + 0x14, (2u << 16) | (4u << 8) | 6u);
    core().mem_w32(kSector + 0x18, 0xFFFFFFFFu); // both animation slots claimed
    const std::uint32_t vertices[6] = {vertexWord(0x00, 0x080, 0x040),
                                       vertexWord(0x00, 0x200, 0x040),
                                       vertexWord(0x40, 0x200, 0x100),
                                       vertexWord(0x40, 0x080, 0x100),
                                       vertexWord(0x80, 0x300, 0x0C0),
                                       vertexWord(0x80, 0x300, 0x180)};
    std::uint32_t at = kSector + 0x1C;
    for (const std::uint32_t word : vertices) {
      core().mem_w32(at, word);
      at += 4;
    }
    for (std::uint32_t i = 0; i < 4; ++i) { // near colours, then the far set
      core().mem_w32(at + 4 * i, 0x00A08060u + i * 0x000A0A0Au);
      core().mem_w32(at + 16 + 4 * i, 0x00402010u + i * 0x00050505u);
    }
    at += 32;
    constexpr std::uint32_t kTwoSidedNeverSplit = 0x84;
    core().mem_w32(at + 0x0, packedIndices(0, 1, 2, 3));
    core().mem_w32(at + 0x4, packedIndices(0, 1, 2, 3));
    core().mem_w32(at + 0xC, kTwoSidedNeverSplit);
    core().mem_w32(at + 0x10, packedIndices(1, 4, 2, 2));
    core().mem_w32(at + 0x14, packedIndices(0, 1, 2, 2));
    core().mem_w32(at + 0x1C, kTwoSidedNeverSplit);
    // Texture 0, shaded then full-bright.
    for (const std::uint32_t pair : {0u, 8u}) {
      core().mem_w32(kTextures + pair + 0, 0x7A200810u);
      core().mem_w32(kTextures + pair + 4, 0x00092010u);
    }
  }

  spyro::test::Spyro1Field field_;
};

// With no turn about x both builders compose the same view; with one they differ, because Spyro 1
// applies Rx last and Spyro 2 first.
void test_the_builder_applies_x_last() {
  Spyro1Scene scene;
  Core &core = scene.core();
  const cam::TrigTables tables{.sine = s1::kRenderGlobals.cameraSineTable,
                               .cosine = s1::kRenderGlobals.cameraCosineTable};
  const cam::Matrices spyro1 = s1::CameraBuilder(core, tables).build({0, 0x123, 0x456});
  const cam::Matrices engine = cam::EngineBuilder(core, tables).build({0, 0x123, 0x456});
  CHECK(spyro1.view == engine.view);
  const cam::Matrices turned = s1::CameraBuilder(core, tables).build({0x300, 0x123, 0x456});
  const cam::Matrices turnedEngine = cam::EngineBuilder(core, tables).build({0x300, 0x123, 0x456});
  CHECK(turned.view != turnedEngine.view);
}

std::int32_t element(std::uint32_t word, bool high) {
  return static_cast<std::int16_t>(high ? word >> 16 : word & 0xFFFFu);
}

// Row 1 of the projection is the view's times 0x140 / 512, rounded toward zero; rows 0 and 2 are
// the view's.
void test_the_builder_scales_row_one_toward_zero() {
  Spyro1Scene scene;
  Core &core = scene.core();
  const cam::TrigTables tables{.sine = s1::kRenderGlobals.cameraSineTable,
                               .cosine = s1::kRenderGlobals.cameraCosineTable};
  bool sawNegativeRemainder = false;
  for (std::int32_t angle = 0; angle < 4096; angle += 97) {
    const cam::Matrices m = s1::CameraBuilder(core, tables).build({angle, angle * 3, angle * 7});
    // Row 1 is m10 (word 1 high), m11 (word 2 low), m12 (word 2 high).
    const std::int32_t view[3] = {
        element(m.view[1], true), element(m.view[2], false), element(m.view[2], true)};
    const std::int32_t projected[3] = {element(m.projection[1], true),
                                       element(m.projection[2], false),
                                       element(m.projection[2], true)};
    for (std::size_t i = 0; i < 3; ++i) {
      const std::int32_t product = view[i] * cam::kVerticalScale;
      CHECK_EQ(projected[i], product / 512);
      sawNegativeRemainder = sawNegativeRemainder || (product < 0 && product % 512 != 0);
    }
    CHECK_EQ(m.projection[0], m.view[0]);
    CHECK_EQ(m.projection[3], m.view[3]);
    CHECK_EQ(m.projection[4], m.view[4]);
    CHECK_EQ(m.projection[1] & 0xFFFFu, m.view[1] & 0xFFFFu);
  }
  CHECK(sawNegativeRemainder);
}

// A sector past the 4:3 frustum's side and inside 16:9's is culled at 4:3 and drawn into the
// margin at 16:9.
void test_the_margin_draws_what_four_three_culls() {
  Spyro1Scene scene;
  scene.shiftSector(0x340);
  const DrawnFrame narrow = scene.drawFrame(spyro::test::kRestCamera, 0);
  CHECK(spyro::test::primitives(narrow.shown).empty());
  scene.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
  const DrawnFrame wide = scene.drawFrame(spyro::test::kRestCamera, 1);
  const auto drawn = spyro::test::primitives(wide.shown);
  CHECK(!drawn.empty());
  bool inMargin = false;
  for (const psx::present::DrawPrimitive &primitive : drawn) {
    for (int i = 0; i < primitive.vertexCount; ++i) {
      inMargin = inMargin || primitive.vertices[i].x < 0 || primitive.vertices[i].x >= 512;
    }
  }
  CHECK(inMargin);
}

// Level code reads the sector marks, so a sector drawn only into the 16:9 margin stays unmarked;
// one inside the 4:3 view is still marked.
void test_the_margin_leaves_the_sector_marks_as_retail() {
  Spyro1Scene scene;
  Core &core = scene.core();
  core.game->guestDisplay.latch(spyro::test::recordWidePlan());
  scene.shiftSector(0x340);
  CHECK(!spyro::test::primitives(scene.drawFrame(spyro::test::kRestCamera, 0).shown).empty());
  CHECK_EQ(core.mem_r8(s1::kFacts.sectorMarks), 0);
  scene.shiftSector(0);
  (void)scene.drawFrame(spyro::test::kRestCamera, 1);
  CHECK_EQ(core.mem_r8(s1::kFacts.sectorMarks), 0xFF);
}

// The drawer is a producer through the runtime: t = 1 is the field as drawn and t = 0.5 is the
// field drawn at the halfway camera, across the two packet pools.
void test_the_composer_draws_the_terrain_between_two_fields() {
  Spyro1Scene scene;
  Core &core = scene.core();
  CHECK(core.game->runtime.stateProducer(core, s1::kSharedFacts.entry) != nullptr);
  const spyro::test::ComposeCheck check = spyro::test::composeAcrossTwoFields(scene);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
  CHECK(check.keepsItsLevel);
}

} // namespace

int main() {
  try {
    RUN(the_builder_applies_x_last);
    RUN(the_builder_scales_row_one_toward_zero);
    RUN(the_margin_draws_what_four_three_culls);
    RUN(the_margin_leaves_the_sector_marks_as_retail);
    RUN(the_composer_draws_the_terrain_between_two_fields);
    return pt_summary();
  } catch (const std::exception &error) {
    lucent::error("test_spyro1_terrain", "FAIL: {}", error.what());
    return 1;
  }
}
