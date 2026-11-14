#include "spyro1/core/frame/draw_cuts.h"

#include "spyro1/core/frame/frame_driver.h"

#include "core.h"
#include "game.h"
#include "native_dispatch.h"

#include <cstdint>

namespace spyro1 {
namespace {

constexpr std::uint32_t kResetSpyro = 0x8004AC24u;

void resetSpyro(Core *core) {
  markDrawCut(*core);
  (void)psx::cpu::callOriginalOrPropagate(*core, kResetSpyro);
}

} // namespace

void markDrawCut(Core &core) {
  if (core.game->frameDriver != nullptr) {
    core.game->frameDriverAs<FrameDriver>().declareCut();
  }
}

void registerDrawCutOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kResetSpyro, "spyro1-spyro-reset-cut", resetSpyro);
}

} // namespace spyro1
