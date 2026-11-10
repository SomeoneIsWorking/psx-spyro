#include "spyro1/core/frame/draw_cuts.h"

#include "core.h"
#include "native_dispatch.h"
#include "replay_frame.h"

#include <cstdint>

namespace spyro1 {
namespace {

constexpr std::uint32_t kCameraForceToDestination = 0x80034C84u;
constexpr std::uint32_t kResetSpyro = 0x8004AC24u;

void cameraForceToDestination(Core *core) {
  markDrawCut(*core);
  (void)psx::cpu::callOriginalOrPropagate(*core, kCameraForceToDestination);
}

void resetSpyro(Core *core) {
  markDrawCut(*core);
  (void)psx::cpu::callOriginalOrPropagate(*core, kResetSpyro);
}

} // namespace

void markDrawCut(Core &core) {
  if (psx::present::ReplayFrame *replay = core.runtime.replayFrame(core)) {
    replay->forget();
  }
}

void registerDrawCutOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, kCameraForceToDestination, "spyro1-camera-force-cut", cameraForceToDestination);
  psx::cpu::installNativeOverride(core, kResetSpyro, "spyro1-spyro-reset-cut", resetSpyro);
}

} // namespace spyro1
