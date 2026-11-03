#include "core/title_logo.h"

#include "core.h"
#include "drawn_logo.h"
#include "game.h"
#include "gpu_device.h"

#include <lucent/log.h>

#include <optional>

namespace spyro {
// The block's tick outlives the title screen (a demo leaves it set), so the game state comes first.
// A null block pointer means the title has not reached its overlay, which is not a failure.
bool titleLogoGateOpen(Core &core, const TitleLogoFacts &facts) {
  if (facts.textures.empty() ||
      core.mem_r32(facts.gate.gameStateGlobal) != facts.gate.titleScreenState) {
    return false;
  }
  if (facts.gate.subStateGlobal != 0 && (core.mem_r32(facts.gate.subStateGlobal) &
                                         facts.gate.subStateMask) != facts.gate.subStateValue) {
    return false;
  }
  if (facts.gate.pointerGlobal == 0) {
    return true;
  }
  const std::uint32_t block = core.mem_r32(facts.gate.pointerGlobal);
  if (block == 0) {
    return false;
  }
  return core.mem_r32(block + facts.gate.tickOffset) >= facts.gate.tickValue;
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
