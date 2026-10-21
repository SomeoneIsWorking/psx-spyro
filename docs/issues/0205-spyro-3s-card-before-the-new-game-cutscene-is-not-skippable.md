# 0205: Spyro 3's card before the new-game cutscene is not skippable

Status: open. Owner: none yet; Spyro 3 has no transition-skip owner (S035).

## Reproduced
Spyro 3, Clang `build/agent`, hidden window (`PSXPORT_VK_HEADLESS=1` in the process environment),
`scratch/0198/s23.py spyro3 noskip`: New Game leaves the title (gamestate 11) into gamestate 5, which shows the
"An Evil Plot Unfolds..." caption over the scene for 255 steps before the first cutscene (gamestate 6)
(`scratch/0198/spyro3-noskip/t00060-gs5.png`..`t00240-gs5.png`, `sheet.png`).
`scratch/0198/s23.py spyro3 skip120 120` taps Start 120 steps into the card: gamestate 6 still starts at +260.

Two more caption cards sit between the intro cutscenes ("A Powerful Villain Emerges...", "A Desperate Rescue
Begins..."), then "THE ADVENTURE BEGINS" (gamestate 5, about 365 steps) before gamestate 3 and gameplay in gamestate 0.

## Gap
The card's gamestate 5 update, what holds it on screen (a timer or the load) and its terminal transition are not
recovered. The skip is title-owned and goes through that terminal, as Spyro 1's does (issue 0198).
