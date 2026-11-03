// debug_menu.h — the debug verbs Spyro 1/2/3 share: the warp catalogue (homeworld, level, arrival)
// and the moby listing. Each title's menu derives from it and adds its own.
#pragma once

#include "core/frame_cut.h"
#include "title_debug_menu.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace spyro {

class MobyTeleport;

// The game states a screen route can reach: gameplay and the pause menu over it by warping, the
// title screen by starting the title.
struct ScreenStates {
  std::uint32_t playing = 0;
  std::uint32_t title = 0;
  std::optional<std::uint32_t> pause;
};

// One level the warp reaches, named as the guest names it, with the arrivals after its start.
struct WarpLevel {
  std::uint32_t id = 0;
  std::string name;
  std::vector<psx::game::WarpPlace> arrivals;
};

struct WarpWorld {
  std::string name;
  std::vector<WarpLevel> levels;
};

// A level as a title's guest tables list it: under the id of the homeworld it belongs to.
struct HomeworldLevel {
  std::uint32_t homeworld = 0;
  WarpLevel level;
};

// The levels under their homeworlds, each homeworld named as its own level and placed where it
// first appears; empty when a homeworld's own level is not among them.
std::vector<WarpWorld> groupByHomeworld(std::vector<HomeworldLevel> levels);

// The title's homeworlds from its guest name tables; empty while the executable is not resident.
using WarpWorlds = std::function<std::vector<WarpWorld>(Core &)>;

// The name a guest pointer word points at; empty when it points nowhere readable.
std::string guestName(Core &core, std::uint32_t pointerWord);

class DebugMenu : public psx::game::TitleDebugMenu {
public:
  // `warp <homeworld>/<level>/<arrival>` over `worlds`, each level's first arrival its start;
  // `mobys` and `moby-teleport` when `mobys` is given. A screen is the game state and level id
  // `scene` names; `states` says which a route reaches.
  DebugMenu(WarpWorlds worlds,
            const MobyTeleport *mobys,
            const FrameCutFacts &scene,
            const ScreenStates &states);

private:
  // "w/l/0" for each level id the catalogue holds.
  std::map<std::uint32_t, std::string> levelPaths_;
};

} // namespace spyro
