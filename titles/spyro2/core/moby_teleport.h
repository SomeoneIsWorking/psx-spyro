#pragma once

#include "core/debug/moby_teleport.h"

namespace spyro2 {

// SCUS_944.25's moby array, its gameplay state and Spyro's position words.
class MobyTeleport final : public spyro::MobyTeleport {
public:
  [[nodiscard]] bool inActiveField(Core &core) const override;

protected:
  [[nodiscard]] std::uint32_t levelMobysWord() const override;
  void placeSpyro(Core &core, const Position &destination) const override;
};

} // namespace spyro2
