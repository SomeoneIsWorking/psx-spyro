#include "spyro1/core/effect/particle_lives.h"

#include "core.h"
#include "spyro1/render/particles.h"

namespace spyro1 {

std::size_t ParticleLives::indexOf(Core &core, std::uint32_t record) {
  return ((record - core.mem_r32(particles::kListBase)) / particles::kRecordBytes) % kSlots;
}

void ParticleLives::allocated(Core &core, std::uint32_t slot) {
  ++lives_[indexOf(core, slot)];
}

std::uint32_t ParticleLives::of(Core &core, std::uint32_t record) const {
  return lives_[indexOf(core, record)];
}

} // namespace spyro1
