#include "guest_terrain_fog.h"

#include "gte_registers.h"

namespace spyro::guest_terrain {

// The GTE register and command numbers are shared vocabulary (psxport gte_registers.h), not this
// pass's.
namespace gte = psx::gte;
void loadFogColour(const TerrainMemory &memory, Core &core, const Facts &facts) {
  core.gte().writeControl(gte::kFarRed, memory.r32(facts.fogColour));
  core.gte().writeControl(gte::kFarGreen, memory.r32(facts.fogColour + 4));
  core.gte().writeControl(gte::kFarBlue, memory.r32(facts.fogColour + 8));
}

std::uint32_t fogColours(TerrainMemory &memory,
                         Core &core,
                         const Facts &facts,
                         std::uint32_t colours,
                         std::uint32_t end,
                         std::uint32_t fog) {
  loadFogColour(memory, core, facts);
  std::uint32_t from = colours;
  core.gte().writeData(gte::kIr0, fog);
  core.gte().writeData(gte::kRgbc, memory.r32(from));
  from += 4;
  std::uint32_t to = facts.foggedColours;
  do {
    core.gte().execute(gte::kDpcs);
    const std::uint32_t next = memory.r32(from);
    from += 4;
    const std::uint32_t faded = core.gte().readData(gte::kRgb2);
    core.gte().writeData(gte::kRgbc, next);
    memory.w32(to, faded);
    to += 4;
  } while (from != end);
  return to;
}

} // namespace spyro::guest_terrain
