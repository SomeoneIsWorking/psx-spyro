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
// Spyro onto node `node` of portal `index`'s path, for the guest's own gate check to act on.
std::string toGate(Core &core, std::uint32_t index, std::uint32_t node);

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
