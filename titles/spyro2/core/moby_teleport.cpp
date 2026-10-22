#include "spyro2/core/moby_teleport.h"

#include "core.h"
#include "spyro2/core/boot_facts.h"
#include "spyro2/render/render_facts.h"

namespace spyro2 {

std::uint32_t MobyTeleport::levelMobysWord() const {
  return kMobyFacts.firstRecordWord;
}

bool MobyTeleport::inActiveField(Core &core) const {
  return core.mem_r32(kGameState) == 0u;
}

void MobyTeleport::placeSpyro(Core &core, const Position &destination) const {
  core.mem_w32(kFieldFacts.spyroPosition, static_cast<std::uint32_t>(destination.x));
  core.mem_w32(kFieldFacts.spyroPosition + 4u, static_cast<std::uint32_t>(destination.y));
  core.mem_w32(kFieldFacts.spyroPosition + 8u, static_cast<std::uint32_t>(destination.z));
}

} // namespace spyro2
