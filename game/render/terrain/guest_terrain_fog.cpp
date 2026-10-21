#include "render/terrain/guest_terrain_fog.h"

#include "gte_registers.h"

namespace spyro::guest_terrain {

namespace gte = psx::gte;
void loadFogColour(const psx::present::EmitMemory &memory, Core &core, const Facts &facts) {
  core.gte().writeControl(gte::kFarRed, memory.mem_r32(facts.fogColour));
  core.gte().writeControl(gte::kFarGreen, memory.mem_r32(facts.fogColour + 4));
  core.gte().writeControl(gte::kFarBlue, memory.mem_r32(facts.fogColour + 8));
}

std::uint32_t fogColours(const psx::present::EmitMemory &memory,
                         Core &core,
                         const Facts &facts,
                         std::uint32_t colours,
                         std::uint32_t end,
                         std::uint32_t fog) {
  loadFogColour(memory, core, facts);
  std::uint32_t from = colours;
  core.gte().writeData(gte::kIr0, fog);
  core.gte().writeData(gte::kRgbc, memory.mem_r32(from));
  from += 4;
  std::uint32_t to = facts.foggedColours;
  do {
    core.gte().execute(gte::kDpcs);
    const std::uint32_t next = memory.mem_r32(from);
    from += 4;
    const std::uint32_t faded = core.gte().readData(gte::kRgb2);
    core.gte().writeData(gte::kRgbc, next);
    memory.mem_w32(to, faded);
    to += 4;
  } while (from != end);
  return to;
}

std::uint32_t fadeColour(Core &core, const ColourFade &colours) {
  const auto [nearColour, farColour, fade] = colours;
  core.gte().writeControl(gte::kFarRed, (nearColour << 4) & 0xFF0u);
  core.gte().writeControl(gte::kFarGreen, (nearColour >> 4) & 0xFF0u);
  core.gte().writeControl(gte::kFarBlue, nearColour >> 12);
  core.gte().writeData(gte::kRgbc, farColour);
  core.gte().writeData(gte::kIr0, fade);
  core.gte().execute(gte::kDpcs);
  return core.gte().readData(gte::kRgb2);
}

} // namespace spyro::guest_terrain
