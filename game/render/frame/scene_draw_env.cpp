#include "scene_draw_env.h"
#include "core.h"
#include "cutscene_scene_recipe.h"
#include "frame_renderer.h"
#include "guest_globals.h"
#include "stage13_scene_recipe.h"

namespace {

using spyro::guest::kGameplayDrawFrame;
using spyro::guest::kTitlescreenState;

bool fieldArm(Core *core, std::uint32_t stage) {
  if (stage == spyro::render::kStageField) {
    return true;
  }
  return (stage == spyro::render::kStageRespawn || stage == spyro::render::kStageGameOver) &&
         core->mem_r32(kGameplayDrawFrame) == 0u;
}

} // namespace

void spyro::render::prepareSceneDrawEnv(Core *core, std::uint32_t stage) {
  // Field arm of 0x8001ED5C, 0x8001E9C8 and 0x8001A050: g_BackgroundColor into both envs.
  if (fieldArm(core, stage) || stage == kStageCutscene || stage == kStageLevelTransition ||
      stage == kStageEntranceAnimation) {
    spyro::cutscene_scene_recipe::prepareFrame(core, spyro::cutscene_scene_recipe::read(core));
    return;
  }
  // 0x8001E6B8 zeroes both background colours ahead of its PutDrawEnv tail.
  if (stage == kStageFrontEnd &&
      !spyro::stage13_scene_recipe::hasSharedBackdrop(core->mem_r32(kTitlescreenState))) {
    spyro::cutscene_scene_recipe::prepareFrame(core, {});
  }
}
