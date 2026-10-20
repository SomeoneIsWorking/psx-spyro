#!/usr/bin/env python3
"""Which pad button a Spyro 1 title screen is asking for, as pure functions both drivers share.

The memory-card FORMAT prompts (TSM_Menu 5/6/7) and the save OVERWRITE prompt (TSM_Loading 2) are
never answered: they only appear over the operator's card content.
"""

from __future__ import annotations

from dataclasses import dataclass

from title_states import (
    GS_CUTSCENE,
    GS_ENTRANCE_ANIMATION,
    GS_EXIT_LEVEL,
    GS_LEVEL_TRANSITION,
    GS_PAUSE_MENU,
    GS_PLAYING,
    GS_TITLE_SCREEN,
    TSS_ACTIVE,
    TSM_DEMO,
    TSM_INIT,
    TSM_LOADING,
    TSM_MENU,
    TitleState,
)

# TSM_Menu sub-states: 15 picks the card, 4 plays without a save, 10 confirms creating the save.
ANSWERABLE_MENU_PROMPTS = frozenset({4, 10, 15})
PRESS_START_PLATFORM = 3  # TSM_Init sub-state: the interactive "PRESS START" platform
NEW_OR_LOAD_PROMPT = 4     # TSM_Loading state: NEW GAME (option 0) beside LOAD GAME
SLOT_PROMPT = 1            # TSM_Loading state: which slot the new game is written into

# The gamestates a route from the save picker into gameplay is allowed to pass through on its way.
LOAD_ROUTE_STATES = frozenset(
    {GS_TITLE_SCREEN, GS_LEVEL_TRANSITION, GS_ENTRANCE_ANIMATION, GS_CUTSCENE})


@dataclass(frozen=True)
class Screen:
    """The guest state a prompt decision may read; `level_trans_hud` is the tally's HUD flag."""

    gamestate: int
    title: TitleState
    level_trans_hud: int = 0

    @property
    def at_save_picker(self) -> bool:
        return self.gamestate == GS_TITLE_SCREEN and self.title.mode == TSM_LOADING

    @property
    def playing(self) -> bool:
        return self.gamestate == GS_PLAYING


@dataclass(frozen=True)
class Prompt:
    buttons: tuple[str, ...] = ()
    reached: bool = False   # this leg of the route is finished
    refuse: str | None = None
    # Fields to hold the buttons when the screen reads a held button, not an edge.
    hold_frames: int = 0
    # The screen this press is for, so a route presses it once; a screen can still read as itself after.
    target: str = ""
    # The press repeats until the guest's own state changes (navigation, a gated modal confirm).
    repeatable: bool = False

    def __bool__(self) -> bool:
        return bool(self.buttons)


def title_menu_prompt(screen: Screen) -> Prompt:
    """Boot -> the memory-card front end; Start only at TSM_Init sub-state 3."""
    if screen.at_save_picker:
        return Prompt(reached=True)
    if screen.gamestate == GS_TITLE_SCREEN and screen.title.mode == TSM_INIT \
            and screen.title.sub_state == PRESS_START_PLATFORM:
        return Prompt(("start",), target="press_start_platform", repeatable=True)
    if screen.gamestate == GS_TITLE_SCREEN and screen.title.mode == TSM_MENU \
            and screen.title.sub_state in ANSWERABLE_MENU_PROMPTS:
        # One target per sub-state; repeated because each gates its decision on m_SubTick >= 8 (titlescreen.c:625).
        return Prompt(("cross",), target=f"title_menu_{screen.title.sub_state}", repeatable=True)
    return Prompt()


def new_game_prompt(screen: Screen) -> Prompt:
    """The save picker -> out of the title screen; NEW GAME is selected explicitly."""
    if screen.gamestate != GS_TITLE_SCREEN:
        return Prompt(reached=True)
    if screen.title.mode == TSM_LOADING and screen.title.state == NEW_OR_LOAD_PROMPT:
        # Moving the selection and confirming it are separate targets.
        if screen.title.option == 0:
            return Prompt(("cross",), target="new_game_confirm", repeatable=True)
        return Prompt(("left",), target="new_game_select", repeatable=True)
    if screen.title.mode == TSM_LOADING and screen.title.state == SLOT_PROMPT:
        return Prompt(("cross",), target="slot_prompt", repeatable=True)
    return Prompt()


# The intro cutscene's skip is a held button gated on layout tick 241 (docs/findings/start-skip-map.md).
CUTSCENE_HOLD_FRAMES = 400


def load_route_prompt(screen: Screen, skip_transitions: bool = False,
                      skip_button: str = "start") -> Prompt:
    """Out of the title screen -> GS_Playing; `skip_transitions` presses `skip_button` on the intro
    cutscene (held), the level-transition tally while its HUD flag is set, and the level flyby."""
    if screen.playing:
        return Prompt(reached=True)
    if skip_transitions and screen.gamestate == GS_CUTSCENE:
        return Prompt((skip_button,), hold_frames=CUTSCENE_HOLD_FRAMES, target="cutscene")
    if skip_transitions and screen.gamestate == GS_LEVEL_TRANSITION and screen.level_trans_hud != 0:
        return Prompt((skip_button,), target="level_transition")
    if skip_transitions and screen.gamestate == GS_TITLE_SCREEN \
            and screen.title.mode == TSM_DEMO and screen.title.state == TSS_ACTIVE:
        return Prompt((skip_button,), target="level_flyby")
    if screen.gamestate not in LOAD_ROUTE_STATES:
        return Prompt(refuse=f"left the load route into unexpected gamestate {screen.gamestate}")
    return Prompt()


# func_8002E12C counts D_80075720 over 0..3; entry 3 calls the exit-level entry func_8002C618.
PAUSE_MAIN_ENTRIES = 4
PAUSE_QUIT_ENTRY = 3
QUIT_SUB_MENU = 0


def pause_quit_prompt(gamestate: int, option: int, sub_menu: int) -> Prompt:
    """Gameplay -> the return-home glide, decided from the menu's own `D_80075720` and `D_800757C8`."""
    if gamestate == GS_EXIT_LEVEL:
        return Prompt(reached=True)
    if gamestate != GS_PAUSE_MENU:
        return Prompt()
    if sub_menu != QUIT_SUB_MENU:
            # The options submenu closes only on Triangle; untargeted, so it may repeat.
        return Prompt(("triangle",))
    if option == PAUSE_QUIT_ENTRY:
        return Prompt(("start",), target="pause_quit_confirm")
    return Prompt(("down",)) if option < PAUSE_QUIT_ENTRY else Prompt(("up",))


