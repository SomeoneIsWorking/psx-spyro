#include "spyro2/core/debug_menu.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/warp.h"
#include "spyro2/render/render_facts.h"

namespace spyro2 {

DebugMenu::DebugMenu()
    : spyro::DebugMenu(
          &Warp::worlds, &mobys_, kBootPrefixFacts.cut, {.title = kTitleScreenGameState}),
      mobys_({.gameState = kGameState,
              .mobysWord = kMobyFacts.firstRecordWord,
              .spyro = kDrawMotion.spyro}) {}

} // namespace spyro2
