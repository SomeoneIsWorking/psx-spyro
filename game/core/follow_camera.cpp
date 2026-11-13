#include "core/follow_camera.h"

#include "core.h"
#include "core/boot_prefix_runtime.h"
#include "core/stick_camera.h"
#include "native_dispatch.h"

namespace spyro {
namespace {

// The camera record both images share (spyro-2 include/camera.h).
constexpr std::uint32_t kMode = 0x4Cu;
constexpr std::uint32_t kTargetElevation = 0x94u; // m_CameraPositions[1].pos.elevation
constexpr std::uint32_t kTargetBias =
    0x9Cu; // m_CameraPositions[1].yaw, where the free look splits to
constexpr std::uint32_t kLocked =
    0x134u; // nonzero: modes 0 and 6 drop the override, mode 1 skips the poll
constexpr std::uint32_t kAzimuthOverride = 0x150u;
constexpr std::uint32_t kRotationState = 0x154u;
constexpr std::uint32_t kRotating = 1u;
constexpr std::uint32_t kPollModes[] = {0u, 1u, 6u};

const FollowCameraFacts &factsOf(const Core &core) {
  return static_cast<const BootPrefixRuntime &>(core.runtime).followCamera();
}

bool pollRefused(Core &core, const FollowCameraFacts &facts) {
  const std::uint32_t held = core.mem_r32(facts.padHeld);
  return (facts.gamestate != 0u && core.mem_r32(facts.gamestate) == facts.refusedGamestate) ||
         (held & facts.refusedHeld) != 0u || core.mem_r32(facts.spyroBusy) != 0u;
}

// The stick takes the camera only where the poll ran unrefused and nothing locked the camera.
bool stickFollows(Core &core, const FollowCameraFacts &facts) {
  const std::uint32_t mode = core.mem_r32(facts.camera + kMode);
  bool polled = false;
  for (const std::uint32_t pollMode : kPollModes) {
    polled = polled || mode == pollMode;
  }
  return polled && core.mem_r32(facts.camera + kLocked) == 0u && !pollRefused(core, facts);
}

// With no shoulder held, the stick's x sets the override and the rotating state as a shoulder press
// does.
void shoulderPoll(Core *core) {
  const FollowCameraFacts &facts = factsOf(*core);
  const auto speed = static_cast<std::int32_t>(core->r[4]);
  if (!psx::cpu::callOriginalOrPropagate(*core, facts.shoulderPoll)) {
    return;
  }
  if (pollRefused(*core, facts) || (core->mem_r32(facts.padHeld) & facts.turnButtons) != 0u) {
    return;
  }
  const std::int32_t yaw = scaledBy(rightStickCamera(*core).x, speed);
  if (yaw != 0) {
    core->mem_w32(facts.camera + kAzimuthOverride, static_cast<std::uint32_t>(yaw));
    core->mem_w32(facts.camera + kRotationState, kRotating);
  }
}

// Row 1 holds the stick's look for the one error computation and gets the game's own target back
// after it.
void errorCompute(Core *core) {
  const FollowCameraFacts &facts = factsOf(*core);
  StickLook look;
  if (stickFollows(*core, facts)) {
    look =
        stickLook(*core, static_cast<std::int32_t>(core->mem_r32(facts.camera + kTargetElevation)));
  }
  const auto shift = [&](std::uint32_t offset, std::int32_t by) {
    core->mem_w32(facts.camera + offset,
                  core->mem_r32(facts.camera + offset) + static_cast<std::uint32_t>(by));
  };
  shift(kTargetElevation, look.elevation);
  shift(kTargetBias, look.bias);
  (void)psx::cpu::callOriginalOrPropagate(*core, facts.errorCompute);
  shift(kTargetElevation, -look.elevation);
  shift(kTargetBias, -look.bias);
}

} // namespace

void registerFollowCamera(Core &core, const FollowCameraFacts &facts) {
  psx::cpu::installNativeOverride(
      core, facts.shoulderPoll, "spyro-shoulder-camera-poll", shoulderPoll);
  psx::cpu::installNativeOverride(core, facts.errorCompute, "spyro-camera-error", errorCompute);
}

} // namespace spyro
