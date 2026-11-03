#include "core/title_logo.h"

#include "core.h"

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

} // namespace spyro
