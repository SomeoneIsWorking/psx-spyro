#pragma once

#include "core/debug/moby_teleport.h"

#include <cstdint>
#include <string>

class Core;

namespace spyro1::teleport {

// Gameplay is in a loaded level: game state 0 with no load stage running.
bool inActiveField(Core &core);
// The level's portals and each one's path nodes, one line each.
std::string listGates(Core &core);
// Spyro's position and rotation bytes.
std::string where(Core &core);
// Spyro at `destination`, through the same position words a portal arrival writes.
std::string toPosition(Core &core, const spyro::MobyTeleport::Position &destination);
// Spyro in front of the homeworld portal into `level`; the reply says why not when no loaded portal
// leads there.
std::string toPortal(Core &core, std::uint32_t level);

// SCUS_942.28's g_LevelMobys, its active field and Spyro's two position copies.
class LevelMobys final : public spyro::MobyTeleport {
public:
  [[nodiscard]] bool inActiveField(Core &core) const override {
    return teleport::inActiveField(core);
  }

protected:
  [[nodiscard]] std::uint32_t levelMobysWord() const override;
  void placeSpyro(Core &core, const Position &destination) const override;
};

} // namespace spyro1::teleport
