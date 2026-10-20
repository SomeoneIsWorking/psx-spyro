# 0198: the transition before the new-game cutscene is not skippable

Status: open

## Observed
USER, 2026-10-20: "only one transition scene is skippable (after the new game cutscene, the one BEFORE
the cutscene is still not skippable)".

Start skips the screen after the new-game cutscene (`TransitionSkip`, issue 0129). The transition from
New Game into the cutscene ignores Start.

## Next
Reproduce in Spyro 1 from the title menu with New Game, and confirm which titles show the same thing.
Name the game state that runs between New Game and the cutscene (Ghidra on its state handler), find its
own terminal transition, and add that screen to `TransitionSkip` through that terminal. If the screen has
no faithful terminal, record that here (see 0138 for the pattern).
