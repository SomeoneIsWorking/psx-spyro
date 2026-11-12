#include "spyro2/core/debug_menu.h"

#include "spyro2/core/boot_facts.h"
#include "spyro2/core/warp.h"
#include "spyro2/render/render_facts.h"

namespace spyro2 {
namespace {

// The cheat matcher 0x800543DC, run by the pause menu 0x800176A4, calls the effect with the matched
// table row; row 1 rolls the credits (0x8001A910, state 10).
constexpr spyro::CreditsCheat kCreditsCheat{.effect = 0x80054518u, .credits = 1u};

} // namespace

DebugMenu::DebugMenu()
    : spyro::DebugMenu(
          &Warp::worlds,
          &mobys_,
          kBootPrefixFacts.cut,
          {.title = kTitleScreenGameState},
          kCreditsCheat),
      mobys_({.gameState = kGameState,
              .mobysWord = kMobyFacts.firstRecordWord,
              .spyro = kDrawMotion.spyro}) {}

} // namespace spyro2
