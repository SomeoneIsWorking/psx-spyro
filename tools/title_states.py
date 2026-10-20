#!/usr/bin/env python3
"""Spyro 1's gamestates, title-overlay modes and states, from external/spyro-1's `include/gamestates.h`
and `include/titlescreen.h`; the one table `drive.py`, `press_conditions.py` and `title_prompts.py` read."""

from __future__ import annotations

from dataclasses import dataclass

# g_Gamestate, dispatched in src/gamestates/update.c.
GS_PLAYING = 0
GS_LEVEL_TRANSITION = 1
GS_PAUSE_MENU = 2
GS_INVENTORY_MENU = 3
GS_RESPAWN = 4
GS_GAME_OVER = 5
GS_OLD_DRAGON = 6
GS_FLIGHT_RESULTS = 7
GS_DRAGON = 8
GS_ENTRANCE_ANIMATION = 9
GS_EXIT_LEVEL = 10
GS_FAIRY = 11
GS_BALLOONIST = 12
GS_TITLE_SCREEN = 13
GS_CUTSCENE = 14
GS_CREDITS = 15

# g_TitlescreenState.m_Mode — which overlay owns the title screen.
TSM_INIT, TSM_MENU, TSM_LOADING, TSM_DEMO = 0, 1, 2, 3

# g_TitlescreenState.m_State, when the mode is TSM_Demo (the flyby card's own phases).
TSS_SETUP, TSS_LOADING, TSS_ACTIVE = 0, 1, 2


@dataclass(frozen=True)
class TitleState:
    """g_TitlescreenState's first six words, read in one `rw`."""
    mode: int
    state: int
    tick: int
    sub_tick: int
    sub_state: int
    option: int


GAMESTATE_NAMES: dict[int, str] = {
    GS_PLAYING: "GS_Playing",
    GS_LEVEL_TRANSITION: "GS_LevelTransition",
    GS_PAUSE_MENU: "GS_PauseMenu",
    GS_INVENTORY_MENU: "GS_InventoryMenu",
    GS_RESPAWN: "GS_Respawn",
    GS_GAME_OVER: "GS_GameOver",
    GS_OLD_DRAGON: "GS_OldDragon",
    GS_FLIGHT_RESULTS: "GS_FlightResults",
    GS_DRAGON: "GS_Dragon",
    GS_ENTRANCE_ANIMATION: "GS_EntranceAnimation",
    GS_EXIT_LEVEL: "GS_ExitLevel",
    GS_FAIRY: "GS_Fairy",
    GS_BALLOONIST: "GS_Balloonist",
    GS_TITLE_SCREEN: "GS_TitleScreen",
    GS_CUTSCENE: "GS_Cutscene",
    GS_CREDITS: "GS_Credits",
}

# What the player sees in each gamestate, in the skip inventory's words.
GAMESTATE_PRESENTATION: dict[int, str] = {
    GS_PLAYING: "gameplay",
    GS_LEVEL_TRANSITION: "level-transition gem tally (portal in/out)",
    GS_PAUSE_MENU: "pause menu",
    GS_INVENTORY_MENU: "inventory / atlas",
    GS_RESPAWN: "respawn spiral",
    GS_GAME_OVER: "game-over spiral",
    GS_OLD_DRAGON: "unused dragon-dialogue stub",
    GS_FLIGHT_RESULTS: "flight-level results",
    GS_DRAGON: "dragon rescue cutscene",
    GS_ENTRANCE_ANIMATION: "level entrance camera sweep",
    GS_EXIT_LEVEL: "return-home glide",
    GS_FAIRY: "fairy save",
    GS_BALLOONIST: "balloonist flight",
    GS_TITLE_SCREEN: "title screen / attract flyby",
    GS_CUTSCENE: "cutscene playback (intro, credits)",
    GS_CREDITS: "credits",
}
