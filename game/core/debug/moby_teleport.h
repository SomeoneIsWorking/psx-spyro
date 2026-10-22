#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <cstdio>
#include <vector>

class Core;

namespace spyro {

// `mobys <class>` and `moby-teleport <class> <n>`: list the level's mobys of one class and place
// Spyro on one, for the guest's own touch or range check to act on (a dragon's rescue, an NPC's
// conversation). Each title names its moby array, its active field and Spyro's position words.
class MobyTeleport {
public:
  struct Position {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
  };

  struct LevelMoby {
    std::uint32_t address = 0;
    Position position;
  };

  virtual ~MobyTeleport() = default;
  MobyTeleport(const MobyTeleport &) = delete;
  MobyTeleport &operator=(const MobyTeleport &) = delete;
  MobyTeleport(MobyTeleport &&) = delete;
  MobyTeleport &operator=(MobyTeleport &&) = delete;

  // Answers `mobys` and `moby-teleport`; false for any other command.
  bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) const;

  // The level's mobys of one class in array order, walked as the guest walks the array.
  [[nodiscard]] std::vector<LevelMoby> ofClass(Core &core, std::uint32_t mobyClass) const;
  void list(Core &core, std::uint32_t mobyClass, std::FILE *reply) const;
  // Spyro onto the `index`th moby of `mobyClass`; false, logged, when gameplay is not in an active
  // field or the level has no such moby.
  bool teleport(Core &core, std::uint32_t mobyClass, std::uint32_t index) const;

protected:
  MobyTeleport() = default;

  // The word holding the level's moby array.
  [[nodiscard]] virtual std::uint32_t levelMobysWord() const = 0;
  // Gameplay is running in a loaded level.
  [[nodiscard]] virtual bool inActiveField(Core &core) const = 0;
  // Every copy of Spyro's position the guest moves him from.
  virtual void placeSpyro(Core &core, const Position &destination) const = 0;
};

} // namespace spyro
