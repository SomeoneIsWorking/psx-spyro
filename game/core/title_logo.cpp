#include "core/title_logo.h"

#include "core.h"
#include "drawn_logo.h"
#include "game.h"
#include "gpu_device.h"

#include <lucent/log.h>

#include <optional>

namespace spyro {
bool titleLogoGateOpen(Core &core, const TitleLogoFacts &facts) {
  const TitleLogoGate &gate = facts.gate;
  if (facts.textures.empty() || core.mem_r32(gate.gameStateGlobal) != gate.titleScreenState) {
    return false;
  }
  if (gate.modeGlobal != 0 && core.mem_r32(gate.modeGlobal) != gate.modeValue) {
    return false;
  }
  return (core.mem_r32(gate.subStateGlobal) & gate.subStateMask) == gate.subStateValue;
}

std::optional<psx::host::LogoImage> extractPanelLogo(Core &core, const TitleLogoFacts &facts) {
  // Shut is the normal answer for all but the moments this title's screen is up; the caller asks
  // again.
  if (core.game == nullptr || !titleLogoGateOpen(core, facts)) {
    return std::nullopt;
  }
  std::optional<psx::host::LogoImage> image = psx::present::drawnLogo(
      core.game->presentation.currentRecord(), core.gpuDevice().vram(), facts.textures);
  if (!image) {
    return std::nullopt;
  }
  lucent::info("picker",
               "the title's own logo rebuilt from its title screen's draws: {}x{}",
               image->width,
               image->height);
  return image;
}

} // namespace spyro
