#include "field_scene_recipe.h"
#include "guest_globals.h"

#include "core.h"

namespace spyro::field_scene_recipe {
namespace {

constexpr uint32_t kFade = 0x80075918u;
constexpr uint32_t kFlightLevel = 0x80075690u;
constexpr uint32_t kPlayerHidden = 0x80075814u;

} // namespace

State read(Core *core) {
  return {
      .environment = {.cameraOcclusionGroup = (int32_t)core->mem_r32(guest::kCameraOcclusionGroup),
                      .occlusionGroupCount =
                          (int32_t)core->mem_r32(guest::kEnvironmentOcclusionGroupCount),
                      .stage = core->mem_r32(guest::kGamestate)},
      .fade = core->mem_r32(kFade),
      .flightLevel = core->mem_r32(kFlightLevel) != 0u,
      .playerVisible = core->mem_r32(kPlayerHidden) == 0u};
}

Plan derive(const State &state) {
  Plan plan{.environment = field_environment::derive(state.environment)};
  plan.layers[plan.layerCount++] = Layer::MobyListBuild;
  if (!state.flightLevel) {
    plan.layers[plan.layerCount++] = Layer::Collectables;
  }
  plan.layers[plan.layerCount++] = Layer::RegularActors;
  if (state.playerVisible) {
    plan.layers[plan.layerCount++] = Layer::PlayerActor;
  }
  plan.layers[plan.layerCount++] = Layer::Environment;
  plan.layers[plan.layerCount++] = Layer::Cyclorama;
  if (state.fade != 0u) {
    plan.layers[plan.layerCount++] = Layer::Fade;
  }
  return plan;
}

void applyEnvironment(Core *core, field_environment::Invocation invocation) {
  for (uint32_t i = 0; i < field_environment::kEdgeWorkAreaSize; ++i) {
    core->mem_w8(field_environment::kEdgeWorkArea + i, 0u);
  }
  core->mem_w32(field_environment::kCullingDistance, invocation.cullingDistance);
}

void applyEnvironment(Core *core, const Plan &plan) {
  applyEnvironment(core, plan.environment);
}

} // namespace spyro::field_scene_recipe
