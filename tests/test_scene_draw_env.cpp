#include "core.h"
#include "frame_renderer.h"
#include "scene_draw_env.h"
#include "testutil.h"

#include <memory>

namespace {

constexpr uint32_t kCycloramaBackground = 0x80078A50u;
constexpr uint32_t kDrawEnvABackground = 0x80076EF9u;
constexpr uint32_t kDrawEnvBBackground = 0x80076F7Du;
constexpr uint32_t kTitleMode = 0x80078D78u;
constexpr uint32_t kGameplayDrawFrame = 0x8007593Cu;

void setBackgrounds(Core &core, uint8_t r, uint8_t g, uint8_t b) {
  for (uint32_t env : {kDrawEnvABackground, kDrawEnvBBackground}) {
    core.mem_w8(env + 0u, r);
    core.mem_w8(env + 1u, g);
    core.mem_w8(env + 2u, b);
  }
}

void checkBackgrounds(Core &core, uint8_t r, uint8_t g, uint8_t b) {
  for (uint32_t env : {kDrawEnvABackground, kDrawEnvBBackground}) {
    CHECK_EQ(core.mem_r8(env + 0u), r);
    CHECK_EQ(core.mem_r8(env + 1u), g);
    CHECK_EQ(core.mem_r8(env + 2u), b);
  }
}

void setCyclorama(Core &core, uint8_t r, uint8_t g, uint8_t b) {
  core.mem_w8(kCycloramaBackground + 0u, r);
  core.mem_w8(kCycloramaBackground + 1u, g);
  core.mem_w8(kCycloramaBackground + 2u, b);
}

// 0x8001E6B8 zeroes both background colours before its PutDrawEnv, so the first attract frame
// after a cutscene clears black rather than with the cutscene's sky.
void test_attract_arm_clears_black_before_the_env_is_programmed() {
  auto core = std::make_unique<Core>();
  setBackgrounds(*core, 0u, 0u, 0xFFu);
  core->mem_w32(kTitleMode, 3u);
  spyro::render::prepareSceneDrawEnv(core.get(), spyro::render::kStageFrontEnd);
  checkBackgrounds(*core, 0u, 0u, 0u);
}

void test_title_menu_keeps_its_background() {
  auto core = std::make_unique<Core>();
  setBackgrounds(*core, 1u, 2u, 3u);
  core->mem_w32(kTitleMode, 0u);
  spyro::render::prepareSceneDrawEnv(core.get(), spyro::render::kStageFrontEnd);
  checkBackgrounds(*core, 1u, 2u, 3u);
}

// The field arm copies g_BackgroundColor after the camera matrices and before its tail.
void test_field_arm_takes_the_cyclorama_colour() {
  auto core = std::make_unique<Core>();
  setBackgrounds(*core, 0u, 0u, 0u);
  setCyclorama(*core, 0x12u, 0x34u, 0x56u);
  spyro::render::prepareSceneDrawEnv(core.get(), spyro::render::kStageField);
  checkBackgrounds(*core, 0x12u, 0x34u, 0x56u);
}

void test_respawn_fade_leaves_the_background() {
  auto core = std::make_unique<Core>();
  setBackgrounds(*core, 7u, 8u, 9u);
  setCyclorama(*core, 0x12u, 0x34u, 0x56u);
  core->mem_w32(kGameplayDrawFrame, 1u);
  spyro::render::prepareSceneDrawEnv(core.get(), spyro::render::kStageRespawn);
  checkBackgrounds(*core, 7u, 8u, 9u);
}

void test_cutscene_arm_takes_the_cyclorama_colour() {
  auto core = std::make_unique<Core>();
  setBackgrounds(*core, 0u, 0u, 0u);
  setCyclorama(*core, 0u, 0u, 0xFFu);
  spyro::render::prepareSceneDrawEnv(core.get(), spyro::render::kStageCutscene);
  checkBackgrounds(*core, 0u, 0u, 0xFFu);
}

} // namespace

int main() {
  RUN(attract_arm_clears_black_before_the_env_is_programmed);
  RUN(title_menu_keeps_its_background);
  RUN(field_arm_takes_the_cyclorama_colour);
  RUN(respawn_fade_leaves_the_background);
  RUN(cutscene_arm_takes_the_cyclorama_colour);
  return pt_summary();
}
