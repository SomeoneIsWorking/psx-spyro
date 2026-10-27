#include "spyro2/core/debug_menu.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/warp.h"

namespace spyro2 {

DebugMenu::DebugMenu()
    : spyro::DebugMenu(
          &Warp::levelExists, &mobys_, kBootPrefixFacts.cut, {.title = kTitleScreenGameState}) {}

} // namespace spyro2
