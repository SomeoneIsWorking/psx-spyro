#pragma once

#include "core/debug/warp.h"

namespace spyro1 {

// SCUS_942.28's warp: from the idle title screen into GamestateCutsceneTransition's TSD_Level
// flyby, the route New Game takes after the intro cutscene (src/gamestates/init.c), which loads
// g_LevelId and hands over to gameplay; from gameplay through the level-warp cheat's transition
// into GS_LevelTransition (src/cheats.c CheatProcessLevelWarp), or from a homeworld through the
// level's portal.
class Warp final : public spyro::Warp {
public:
  // Homeworlds 10..60, six levels each, Gnasty's World five: the bounds the retail level-warp cheat
  // checks (src/cheats.c CheatProcessLevelWarp).
  [[nodiscard]] static bool levelExists(std::uint32_t level);

protected:
  // GS_TitleScreen, TSM_Init, sub-state 3: Spyro on the platform, waiting for Start or the demo
  // timeout.
  [[nodiscard]] bool titleScreenIdle(Core &core) const override;
  // func_8001277C resets progress as New Game does, then g_LevelId and the TSD_Level handoff.
  void startLevel(Core &core, std::uint32_t level) const override;
  // GS_Playing with no load running, where CheatsProcess takes input.
  [[nodiscard]] bool levelChangeReady(Core &core) const override;
  // From the level's homeworld, its portal's trigger region (DispatchSpyroTriggerEvent) with Spyro in
  // front of the portal; otherwise CheatProcessLevelWarp with the level's homeworld selected and its
  // button down for that call.
  void changeLevel(Core &core, std::uint32_t level) const override;
};

} // namespace spyro1
