#pragma once

class Core;

namespace spyro1 {

// GS_LevelTransition's update func_8002DF9C (0x8002DF9C), native: Spyro's flight, the camera and
// the tally as the guest runs them, then LoadLevel(1) under the guest's own tally gate, called
// again in the same update while its last call moved the load on (spyro::drainLoader).
void registerLevelTransitionUpdate(Core &core);

} // namespace spyro1
