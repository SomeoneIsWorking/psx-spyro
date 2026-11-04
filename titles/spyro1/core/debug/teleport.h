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
// A heading's units per turn.
inline constexpr std::uint32_t kHeadingTurn = 0x1000u;
// Spyro's position and heading.
std::string where(Core &core);
// Spyro at `destination` facing `heading` (12 bits a turn), through the position words a portal
// arrival writes and the heading a level spawn sets; the camera settles behind him.
std::string
toPosition(Core &core, const spyro::MobyTeleport::Position &destination, std::uint32_t heading);
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
