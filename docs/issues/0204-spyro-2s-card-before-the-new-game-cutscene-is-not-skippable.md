# 0204: Spyro 2's card before the new-game cutscene is not skippable

Status: open. Owner: none yet; Spyro 2 has no transition-skip owner (S034).

## Reproduced
Spyro 2, Clang `build/agent`, hidden window (`PSXPORT_VK_HEADLESS=1` in the process environment),
`scratch/0198/s23.py spyro2 noskip`: New Game leaves the title (gamestate 11) into gamestate 5, which shows the
"In the world of Dragons" storybook card for 190 steps before the first cutscene (gamestate 6)
(`scratch/0198/spyro2-noskip/t00005-gs5.png`..`t00180-gs5.png`, `sheet.png`).
`scratch/0198/s23.py spyro2 skip100 100` taps Start 100 steps into the card: gamestate 6 still starts at +190.

The same storybook presentation follows the first cutscene ("Meanwhile, in the world of Avalar", 285 steps) and the
second ("The adventure begins", 490 steps, then gamestate 3 and gameplay in gamestate 0).

## Gap
The card's gamestate 5 update, what holds it on screen (a timer or the load) and its terminal transition are not
recovered. The skip is title-owned and goes through that terminal, as Spyro 1's does (issue 0198).
