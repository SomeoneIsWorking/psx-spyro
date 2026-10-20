#include "spyro1/core/input/input_phase.h"

#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {

std::uint64_t InputPhase::of(Core &core) const {
  return pack(core.mem_r32(guest::kGamestate), core.mem_r32(guest::kLevelId));
}

} // namespace spyro1
