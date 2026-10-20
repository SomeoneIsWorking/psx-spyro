#!/usr/bin/env python3
"""title_prompts.py — WHICH pad button a given title screen is asking for, as a pure function.

Two drivers now play this game: `drive.py`, which blocks the product between commands over the
framework REPL, and `live_play.py`, which drives it over the live debug server while it keeps running.
The menu sequence is title knowledge, not transport knowledge, so it lives here once and both drivers
ask it. A second copy in the live driver would be free to answer a different prompt than the REPL one,
and the difference would show up as "the live run could not reach gameplay" with nothing to compare.

Each function takes a `Screen` — the guest state it is allowed to look at — and returns a `Prompt`:
the buttons to tap now, and/or the refusal that says this screen is not one this route can answer.

WHAT IS DELIBERATELY NOT ANSWERED, in both drivers, because answering it would destroy the operator's
data to reach a screenshot: the memory-card FORMAT prompts (TSM_Menu sub-states 5/6/7) and the save
OVERWRITE prompt (TSM_Loading state 2). Both only appear over existing card content.
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

# TSM_Menu sub-states that are safe to answer: 15 picks the card, 4 accepts playing without a save, 10
# confirms creating this game's save. The FORMAT prompts (5/6/7) are absent on purpose.
ANSWERABLE_MENU_PROMPTS = frozenset({4, 10, 15})
PRESS_START_PLATFORM = 3  # TSM_Init sub-state: the interactive "PRESS START" platform
NEW_OR_LOAD_PROMPT = 4     # TSM_Loading state: NEW GAME (option 0) beside LOAD GAME
SLOT_PROMPT = 1            # TSM_Loading state: which slot the new game is written into

# The gamestates a route from the save picker into gameplay is allowed to pass through on its way.
LOAD_ROUTE_STATES = frozenset(
    {GS_TITLE_SCREEN, GS_LEVEL_TRANSITION, GS_ENTRANCE_ANIMATION, GS_CUTSCENE})


@dataclass(frozen=True)
class Screen:
    """The guest state a prompt decision may read. `level_trans_hud` is the level-transition tally's
    own HUD flag, which is both what the port checks and what says the screen is still up."""

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
    # How many FIELDS to hold the buttons, when the screen's own route needs a held button rather
    # than an edge. The intro cutscene is the case: the guest tests `g_Pad.m_Held & (START|CROSS)`
    # and only rewrites the layout's duration once the tick is past 241, so a four-field edge lands
    # before the gate opens and changes nothing. A held button is still a player's button.
    hold_frames: int = 0
    # WHICH screen this prompt is for, so a caller that walks a route can press it ONCE. A prompt with
    # no target is either a refusal or the end of a leg and is never a press to repeat. The rule is not
    # tidiness: `load_route_prompt` is asked once per observation step, and after a cancellation the
    # guest's own screen struct can still read like the card it just left -- a re-fire is then a press
    # with nothing to cancel. With Start that is the pause button: the re-fired press opened GS_PauseMenu
    # during the load route in a live run (exit 2, `drive.py REFUSED: left the load route into
    # unexpected gamestate 2`), and the same re-fire with Cross does nothing at all, which is worse --
    # it looks like a pass.
    target: str = ""
    # A prompt whose OWN screen keeps asking for the same press until the guest's state says
    # otherwise, which is what navigation is: the save picker's Left repeats until its option word
    # reads NEW GAME. A prompt that is ONE decision about ONE presentation must NOT say so, because
    # after the guest has left the presentation its struct can still read like the screen, and the
    # repeat then lands in whatever came next.
    repeatable: bool = False

    def __bool__(self) -> bool:
        return bool(self.buttons)


def title_menu_prompt(screen: Screen) -> Prompt:
    """Boot -> the memory-card front end. Start is only meaningful at TSM_Init sub-state 3; every
    other TSM_Init sub-state is the fly-in, a fade, or the demo hand-off, where a press would either do
    nothing or cancel the attract sequence."""
    if screen.at_save_picker:
        return Prompt(reached=True)
    if screen.gamestate == GS_TITLE_SCREEN and screen.title.mode == TSM_INIT \
            and screen.title.sub_state == PRESS_START_PLATFORM:
        return Prompt(("start",), target="press_start_platform", repeatable=True)
    if screen.gamestate == GS_TITLE_SCREEN and screen.title.mode == TSM_MENU \
            and screen.title.sub_state in ANSWERABLE_MENU_PROMPTS:
        # One press per MENU SCREEN, named by its sub-state: the front end walks through several in a
        # row, so a single target for the whole menu would answer exactly one of them and stall.
        # REPEATABLE, unlike the picker's confirms. These sub-states gate their own decision on
        # `m_SubTick >= 8` (TSM_Menu's CreateSaveConfirm, titlescreen.c:625), so a press delivered
        # before the gate opens changes nothing; measured on a blank card, one press at frame 1760
        # into TS_SubState_CreateSaveConfirm left the guest asking the same question for the rest of
        # the 12000-field budget, and repeating the press carried it through to gameplay in 40. The
        # screen is modal and the press is idempotent on it: either the sub-state advances and the
        # target changes with it, or nothing happened at all.
        return Prompt(("cross",), target=f"title_menu_{screen.title.sub_state}", repeatable=True)
    return Prompt()


def new_game_prompt(screen: Screen) -> Prompt:
    """The save picker -> out of the title screen. NEW GAME must be selected explicitly: confirming
    the picker on its default LOAD GAME over empty slots has nothing to load and bounces back to the
    title, which is what silently ended earlier runs."""
    if screen.gamestate != GS_TITLE_SCREEN:
        return Prompt(reached=True)
    if screen.title.mode == TSM_LOADING and screen.title.state == NEW_OR_LOAD_PROMPT:
        # The option is already NEW GAME: confirm. Otherwise move to it first. Two TARGETS, not one:
        # moving the selection is a navigation that must be free to repeat until the guest's own
        # option word reads NEW GAME, while the confirm is a single decision. Keyed as one target the
        # picker either loses its navigation (one Left, then never again) or repeats its confirm.
        if screen.title.option == 0:
            return Prompt(("cross",), target="new_game_confirm", repeatable=True)
        return Prompt(("left",), target="new_game_select", repeatable=True)
    if screen.title.mode == TSM_LOADING and screen.title.state == SLOT_PROMPT:
        return Prompt(("cross",), target="slot_prompt", repeatable=True)
    return Prompt()


# The intro cutscene's own skip is a HELD button, gated on the layout tick reaching 241 before the
# last 32 samples, and it works by shortening the duration rather than by jumping anywhere (see
# docs/findings/start-skip-map.md, "Still unclassified"). 400 fields covers the 241-tick gate and the
# ~32 fields the shortened layout then needs to reach its own terminal, with room for the press to
# be issued a little after the state was entered.
CUTSCENE_HOLD_FRAMES = 400


def load_route_prompt(screen: Screen, skip_transitions: bool = False,
                      skip_button: str = "start") -> Prompt:
    """Out of the title screen -> GS_Playing.

    `skip_transitions` presses `skip_button` on every presentation on this route that the game itself
    can be asked to end, and each one is that screen's OWN route rather than a host shortcut:

    * the intro cutscene (GS_Cutscene), where the guest shortens the layout while Start or Cross is
      HELD — held, because the guest gates the rewrite on tick >= 241, which an edge never reaches;
    * the level-transition tally (GS_LevelTransition), but only while its HUD flag is still set, and
    * the "THE ADVENTURE BEGINS..." flyby on sight — including while the level is still streaming,
      because the port holds an early press until its load gate opens.

    The button is a parameter because the two buttons are not interchangeable evidence: retail
    accepts Start OR Cross on every one of these, and a run that only ever pressed Start would leave
    the Cross half of the claim untested. A run with the flag off and a run with it on differ by
    nothing but the press.
    """
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


# The pause menu's own quit route, which is how a sub-level's return-home glide (gamestate 10) is
# reached at all. QUIT is the fourth main entry -- `func_8002E12C` counts D_80075720 over 0..3 with
# Start or Cross as confirm, and index 3 is the arm that calls `func_8002C618`, the exit-level entry.
PAUSE_MAIN_ENTRIES = 4
PAUSE_QUIT_ENTRY = 3
QUIT_SUB_MENU = 0


def pause_quit_prompt(gamestate: int, option: int, sub_menu: int) -> Prompt:
    """Gameplay -> the return-home glide, one observed menu field at a time.

    `option` is the guest's own `D_80075720` and `sub_menu` its `D_800757C8`, so the decision is made
    from what the menu currently has selected rather than from a fixed count of Down taps: the
    option index the driver assumes and the one the guest holds are the same word, and a route that
    counted presses would be one field of drift away from opening the options submenu instead.
    """
    if gamestate == GS_EXIT_LEVEL:
        return Prompt(reached=True)
    if gamestate != GS_PAUSE_MENU:
        return Prompt()
    if sub_menu != QUIT_SUB_MENU:
        # The options submenu is open: the guest reads Triangle to back out of it, and nothing else
        # closes it. This is the state a blind Down-count would walk into and stay in. No target: this
        # prompt answers a menu that is walked entry by entry, so it must be free to repeat, which is
        # exactly what the one-press rule the cancellation prompts carry would forbid.
        return Prompt(("triangle",))
    if option == PAUSE_QUIT_ENTRY:
        return Prompt(("start",), target="pause_quit_confirm")
    return Prompt(("down",)) if option < PAUSE_QUIT_ENTRY else Prompt(("up",))


