#!/usr/bin/env python3
"""Four Spyro 1 scenes driven from the guest's own words: a flight level, a boss, a death and the save.

Each scene refuses by name, with the words it read, when its target is not reached.

  flight-level   Sunny Flight (level 15): g_IsFlightLevel set, then GS_Playing.
  boss-level     Toasty (level 14): the guest's own g_LevelIndex, then GS_Playing.
  death-respawn  the guest's own death (func_8002C85C sets GS_Respawn/GS_GameOver), then GS_Playing.
  save-fairy     the fairy's card write (func_800314B4): pages 2 then 7 ("GAME SAVED").

Usage:
    uv run --frozen python tools/route_scenes.py --list
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from typing import Callable

import guest_globals
import repl_route
import repl_walk
import spyro1_steering
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from spyro1_steering import moby_class_targets

# Addresses from external/spyro-1 (byte-identical SCUS_942.28).

# g_IsFlightLevel (common.h).
G_IS_FLIGHT_LEVEL = 0x80075690
# g_SpyroLifeCount (common.h).
G_LIFE_COUNT = 0x8007582C
# g_Spyro.m_Position.
G_SPYRO_X = guest_globals.kSpyro
G_SPYRO_Y = guest_globals.kSpyro + 0x04
G_SPYRO_Z = guest_globals.kSpyro + 0x08
# g_Spyro.m_health (func_8004A4B0); corroboration only, never the death condition.
G_SPYRO_HEALTH = guest_globals.kSpyro + 0x80
# The body state func_8004A200 switches on; 0x1D..0x1F are the liquid states.
G_SPYRO_STATE = guest_globals.kPlayerState
# g_FairyCutscene (fairy.h), immediately after g_Spyro.
G_FAIRY_STATE = 0x80078D00
# +0x08 is the menu option on page 0 and the card machine's stage after it (0x80031E74, 0x800320C8).
G_FAIRY_OPTION = 0x80078D00 + 0x08   # m_MenuSelectedOption (0=SAVE, 1=RETRY, 2=CONTINUE) on page 0
G_FAIRY_CARD_STAGE = 0x80078D00 + 0x08  # the card machine's stage on pages 1..7, the same word
G_FAIRY_PAGE = 0x80078D00 + 0x0C     # m_MenuDialoguePage, the 0..7 page the update dispatches on
G_FAIRY_HAS_CARD = 0x80078D00 + 0x14  # m_HasMemoryCard; 0 sends the menu to CONTINUE, not SAVE

# Moby classes from the level-10 overlay's dispatcher, func_level_10_8007D9C8.
FAIRY_MOBY_CLASS = 110   # 0x8007DB54; its block ends in `jal InitFairyCutscene` (0x80080A1C)
BALLOONIST_MOBY_CLASS = 187  # 0x8007DBAC

# common.h's Gamestate values these scenes prove.
GS_RESPAWN = 4
GS_GAME_OVER = 5
GS_FAIRY = 11
# func_8004A4F4 `slti $v0,$v0,0x400`: below this Z the guest calls func_8002C85C.
DEATH_PLANE_Z = 0x400
# func_800314B4's pages: 2 reaches SaveCreate and MemCardWriteFile, 7 is "GAME SAVED", 3..6 are failures.
SAVE_PAGE_WRITE = 2
SAVE_PAGE_SAVED = 7
SAVE_PAGE_FAILED_FIRST = 3

# g_LevelId/10-1 is the homeworld and g_LevelId%10 the slot (4 = boss, 5 = flight).
BOSS_LEVEL_ID = 14
FLIGHT_LEVEL_ID = 15
# g_LevelIndex (common.h), the guest's (homeworld * 6) + slot (loaders.c:1398).
G_LEVEL_INDEX = 0x80075964
# Artisans is homeworld 0, so the index is the slot.
BOSS_LEVEL_SLOT = 14 % 10
FLIGHT_LEVEL_SLOT = 15 % 10
BOSS_LEVEL_SLOT = 14 % 10
FLIGHT_LEVEL_SLOT = 15 % 10


@dataclass(frozen=True)
class Proof:
    """What a scene proved, the steps it spent and the guest words read at the proof."""

    scene: str
    target: str
    frames: int
    words: dict[str, int | str] = field(default_factory=dict)
    detail: str = ""

    def as_json(self) -> dict:
        return {
            "scene": self.scene,
            "target": self.target,
            "frames": self.frames,
            "words": {name: (f"0x{value:08X}" if isinstance(value, int) else value)
                     for name, value in self.words.items()},
            "detail": self.detail,
        }


class ScenePort:
    """The slice of the product a scene drives, plus the readers every scene needs."""

    def __init__(self, port, fields_at_entry: int):
        self._port = port
        self.entered = fields_at_entry

    @property
    def raw(self):
        """The product object itself, for the walk stepper."""
        return self._port

    @property
    def frame(self) -> int:
        return self._port.frame

    def words(self, address: int, count: int = 1) -> list[int]:
        return self._port.words(address, count)

    def word(self, address: int) -> int:
        return self._port.words(address, 1)[0]

    def press(self, button: str) -> None:
        self._port.press(button)

    def release(self, button: str) -> None:
        self._port.release(button)

    def tap(self, button: str, frames: int = 4) -> None:
        self._port.tap(button, frames)

    def run(self, frames: int) -> None:
        self._port.run(frames)

    def spent(self) -> int:
        return self._port.frame - self.entered

    def signed(self, address: int) -> int:
        value = self.word(address)
        return value - 0x100000000 if value >= 0x80000000 else value


def _walk_until(port: ScenePort, what: str, targets, predicate, description: str,
                budget: int, arrived: int | None = 0, route: str | None = None,
                from_player: bool = False) -> int:
    """Walk until `predicate` holds, following the recorded `route` first when this level has one.

    `arrived=0` keeps walking through a portal; `from_player` measures from Spyro instead of the camera.
    """
    if route is not None:
        recorded = repl_route.Route.find(port.word(guest_globals.kLevelId), route)
        if recorded is not None:
            repl_route.follow(port.raw, recorded, stop=predicate, stop_is=description)
    return repl_walk.Seeker(port.raw, what, targets, budget=budget, arrived=arrived,
                            stop=predicate, stop_is=description, from_player=from_player).walk()


def flight_level(port: ScenePort) -> Proof:
    """Walk into Sunny Flight's portal and prove g_IsFlightLevel is set; docs/issues/0170 tracks the route."""
    portals = spyro1_steering.portal_targets(port.words, level=FLIGHT_LEVEL_ID)
    if not portals:
        available = [target.what for target in spyro1_steering.portal_targets(port.words)]
        raise ToolError(
            f"this homeworld has no portal to level {FLIGHT_LEVEL_ID} (Sunny Flight); it offers "
            f"{', '.join(available) or 'no portals at all'}, so the flight route needs a homeworld "
            "whose flight portal is unlocked, not a different input plan"
        )
    _walk_until(port, f"portal to level {FLIGHT_LEVEL_ID}", portals,
                lambda: port.word(guest_globals.kLevelId) == FLIGHT_LEVEL_ID,
                f"left the homeworld for level {FLIGHT_LEVEL_ID}", budget=9000,
                route=f"portal-{FLIGHT_LEVEL_ID}")
    if not _await(port, lambda: port.word(G_IS_FLIGHT_LEVEL) != 0, "g_IsFlightLevel to be set",
                  budget=1800):
        raise ToolError(
            f"entered level {FLIGHT_LEVEL_ID} but g_IsFlightLevel (0x{G_IS_FLIGHT_LEVEL:08X}) is "
            f"still {port.word(G_IS_FLIGHT_LEVEL)} after {port.spent()} step(s): the level loaded "
            "and the flight flag did not, which is a level-loading defect rather than a route one"
        )
    index = port.word(G_LEVEL_INDEX)
    if index != FLIGHT_LEVEL_SLOT:
        raise ToolError(
            f"g_LevelId is {FLIGHT_LEVEL_ID} but g_LevelIndex is {index}, and the guest's own "
            f"derivation of it for Artisans is {FLIGHT_LEVEL_SLOT} (loaders.c:1398), so the level "
            "index was not computed from the level that loaded"
        )
    # The flight level's own code runs after the hand-off, so wait for GS_Playing.
    if not _await(port, lambda: port.word(guest_globals.kGamestate) == 0,
                  "GS_Playing in the flight level", budget=2400):
        raise ToolError(
            f"g_IsFlightLevel is set but the guest never returned to GS_Playing (gamestate "
            f"{port.word(guest_globals.kGamestate)}) after {port.spent()} step(s), so the level "
            "loaded and the hand-off to the player did not"
        )
    return Proof(
        "flight-level",
        f"g_IsFlightLevel=0x{port.word(G_IS_FLIGHT_LEVEL):08X} with "
        f"g_LevelId={port.word(guest_globals.kLevelId)} (Sunny Flight), the guest's own "
        f"g_LevelIndex={index}, at GS_Playing",
        port.spent(),
        {
            "g_IsFlightLevel": port.word(G_IS_FLIGHT_LEVEL),
            "g_LevelId": port.word(guest_globals.kLevelId),
            "g_LevelIndex": index,
            "g_Gamestate": port.word(guest_globals.kGamestate),
        },
    )


def boss_level(port: ScenePort) -> Proof:
    """Walk into the boss portal and prove the guest loaded Toasty; docs/issues/0170 tracks the route."""
    portals = spyro1_steering.portal_targets(port.words, level=BOSS_LEVEL_ID)
    if not portals:
        available = [target.what for target in spyro1_steering.portal_targets(port.words)]
        raise ToolError(
            f"this homeworld has no portal to level {BOSS_LEVEL_ID} (Toasty); it offers "
            f"{', '.join(available) or 'no portals at all'}, so the boss route needs a homeworld "
            "whose boss portal is unlocked, not a different input plan"
        )
    _walk_until(port, f"portal to level {BOSS_LEVEL_ID}", portals,
                lambda: port.word(guest_globals.kLevelId) == BOSS_LEVEL_ID,
                f"left the homeworld for level {BOSS_LEVEL_ID}", budget=9000,
                route=f"portal-{BOSS_LEVEL_ID}")
    if not _await(port, lambda: port.word(guest_globals.kGamestate) == 0,
                  "GS_Playing in the boss level", budget=1800):
        raise ToolError(
            f"entered level {BOSS_LEVEL_ID} but the guest never returned to GS_Playing "
            f"(gamestate {port.word(guest_globals.kGamestate)}) after {port.spent()} step(s)"
        )
    # The guest's own index arithmetic, not just the level id word.
    index = port.word(G_LEVEL_INDEX)
    if index != BOSS_LEVEL_SLOT:
        raise ToolError(
            f"g_LevelId is {BOSS_LEVEL_ID} but g_LevelIndex is {index}, and the guest's own "
            f"derivation of it for Artisans is {BOSS_LEVEL_SLOT}: the level id was reached without "
            "the level index being computed from it, so nothing here proves a level loaded"
        )
    return Proof(
        "boss-level",
        f"g_LevelId={port.word(guest_globals.kLevelId)} (Toasty) with the guest's own "
        f"g_LevelIndex={index}, at GS_Playing",
        port.spent(),
        {
            "g_LevelId": port.word(guest_globals.kLevelId),
            "g_LevelIndex": index,
            "g_Gamestate": port.word(guest_globals.kGamestate),
            "g_IsFlightLevel": port.word(G_IS_FLIGHT_LEVEL),
        },
    )


# Level 10's west pond drowns Spyro; the recorded "pond" route wades in, else the walk heads west.
DEATH_BUDGET = 12000


def death_respawn(port: ScenePort) -> Proof:
    """Walk west until the guest kills Spyro (func_8002C85C), then let the respawn finish."""
    lives_before = port.signed(G_LIFE_COUNT)
    portals = spyro1_steering.portal_targets(port.words)
    if not portals:
        raise ToolError("this level carries no portal table, so the death route has no water to cross")
    target = min(portals, key=lambda portal: portal.position[0])
    lowest = port.signed(G_SPYRO_Z)
    died = lambda: port.word(guest_globals.kGamestate) in (GS_RESPAWN, GS_GAME_OVER)
    try:
        _walk_until(port, target.what, [target], died, "the guest killed Spyro",
                    budget=DEATH_BUDGET, route="pond")
    except ToolError as refusal:
        raise ToolError(
            f"walking at {target.what} {target.position} the guest never killed Spyro: "
            f"g_Gamestate stayed {port.word(guest_globals.kGamestate)} (death plane Z "
            f"{DEATH_PLANE_Z}; refusal: {refusal})"
        ) from refusal
    state = port.word(guest_globals.kGamestate)
    if not died():
        raise ToolError(
            f"the walk at {target.what} ended with g_Gamestate {state}, which is not the guest's "
            f"GS_Respawn ({GS_RESPAWN}) or GS_GameOver ({GS_GAME_OVER}): arriving is not dying"
        )
    # The respawn restores health, so read it on the death field.
    health = port.word(G_SPYRO_HEALTH)
    body_state = port.word(G_SPYRO_STATE)
    lowest = min(lowest, port.signed(G_SPYRO_Z))
    return _death_proof(port, target.what, port.spent(), lives_before, lowest, state, health,
                         body_state)


def _death_proof(port: ScenePort, sector: str, fields: int, lives_before: int,
                 lowest: int, state: int, health: int, body_state: int) -> Proof:
    """Wait for GS_Playing after the respawn and report the life count the guest decremented."""
    if not _await(port, lambda: port.word(guest_globals.kGamestate) == 0,
                  "GS_Playing after the respawn", budget=1800):
        raise ToolError(
            f"the guest entered gamestate {state} (the death) but never returned to GS_Playing "
            f"after {port.spent()} step(s); it is still {port.word(guest_globals.kGamestate)}, "
            "so the respawn never completed"
        )
    return Proof(
        "death-respawn",
        f"g_Gamestate went to {state} ("
        f"{'GS_Respawn' if state == GS_RESPAWN else 'GS_GameOver'}) walking {sector}, and Spyro is "
        f"back at GS_Playing with {port.signed(G_LIFE_COUNT)} life/lives from {lives_before}",
        port.spent(),
        {
            "death_gamestate": state,
            "death_health": health,
            "death_body_state": body_state,
            "death_z": lowest,
            "lives_before": lives_before,
            "lives_after": port.signed(G_LIFE_COUNT),
            "g_LevelId": port.word(guest_globals.kLevelId),
        },
        detail=(f"the guest killed Spyro walking {sector} in body state {body_state:#x}; lowest Z {lowest} "
                f"(death plane {DEATH_PLANE_Z}, liquid timers in func_8004A200)"),
    )


# func_800314B4 page 0: guest DOWN (0x4000) moves the option, CROSS (0x40) on option 0 opens page 2;
# input is ignored until its timer reaches 8 (0x80031EF4).
FAIRY_MENU_OPTION = 0
# Counts are product steps, the REPL's `run N` unit, two fields each in gameplay.
FAIRY_MENU_WATCH_STEPS = 30
FAIRY_CARD_STEPS = 900
FAIRY_MENU_STEPS = 24
# The card machine runs page 0 -> 2 -> 7 in about 26 fields, so it is sampled every step.
FAIRY_WATCH_STEPS = 1


# The trigger radius is 0x400 world units (4 per view unit); she wanders, so the approach repeats.
FAIRY_STAND_VIEW_DISTANCE = 150
FAIRY_STAND_STEPS = 23
FAIRY_APPROACH_ATTEMPTS = 30
FAIRY_APPROACH_STEPS = 450


def _approach_fairy(port: ScenePort, opened, fairies) -> None:
    """Stand Spyro still inside the wandering fairy's trigger (OctDistance < 0x400, |dz| < 0x200)."""
    route = f"fairy-{FAIRY_MOBY_CLASS}"
    for attempt in range(FAIRY_APPROACH_ATTEMPTS):
        targets = moby_class_targets(port.words, FAIRY_MOBY_CLASS) or fairies
        try:
            _walk_until(port, f"class {FAIRY_MOBY_CLASS} (the fairy)", targets, opened,
                        "the fairy cutscene opened", budget=FAIRY_APPROACH_STEPS,
                        arrived=FAIRY_STAND_VIEW_DISTANCE, route=route if attempt == 0 else None,
                        from_player=True)
        except ToolError:
            if opened():
                return
            raise
        if opened() or _await(port, opened, "the fairy cutscene to open once Spyro stands still",
                              budget=FAIRY_STAND_STEPS):
            return
        print(f"fairy: attempt {attempt + 1} stood still without the trigger firing", file=sys.stderr)
    fairy = moby_class_targets(port.words, FAIRY_MOBY_CLASS)[0].position
    raise ToolError(
        f"{FAIRY_APPROACH_ATTEMPTS} approach(es) to within {FAIRY_STAND_VIEW_DISTANCE} view units of "
        f"the fairy, each followed by {FAIRY_STAND_STEPS} step(s) standing still, and g_Gamestate "
        f"stayed {port.word(guest_globals.kGamestate)}: the trigger (OctDistance < 0x400, |dz| < "
        f"0x200, body state 0 or 0xD, health > 0) was not met; Spyro is at "
        f"({port.signed(G_SPYRO_X)}, {port.signed(G_SPYRO_Y)}, {port.signed(G_SPYRO_Z)}) in body "
        f"state {port.word(G_SPYRO_STATE):#x}; the fairy is now at {fairy}"
    )


def save_fairy(port: ScenePort) -> Proof:
    """Walk into the fairy's proximity trigger (0x80080788), answer her menu, and prove the card write."""
    fairies = moby_class_targets(port.words, FAIRY_MOBY_CLASS)
    if not fairies:
        raise ToolError(
            f"this level carries no Moby of class {FAIRY_MOBY_CLASS}, which is the class the level "
            "overlay's own dispatcher calls InitFairyCutscene for, so there is nobody here to save"
        )
    opened = lambda: port.word(guest_globals.kGamestate) == GS_FAIRY
    _approach_fairy(port, opened, fairies)
    has_card = port.word(G_FAIRY_HAS_CARD)
    page_before, option = _answer_menu(port, has_card)
    return _await_write(port, page_before, option, has_card)


def _answer_menu(port: ScenePort, has_card: int) -> tuple[int, int]:
    """Put the fairy's menu on SAVE and take it; returns the page and option the guest took it on.

    From page 1 on the option word is the card stage, so the page is checked first.
    """
    option = port.signed(G_FAIRY_OPTION)
    for step in range(FAIRY_MENU_STEPS):
        page = port.word(G_FAIRY_PAGE)
        if page != 0:
            return page, option
        option = port.signed(G_FAIRY_OPTION)
        if option != FAIRY_MENU_OPTION:
            print(f"save: menu option is {option}, moving it to {FAIRY_MENU_OPTION}", file=sys.stderr)
            port.tap("down", 8)
        else:
            print(f"save: pressing CROSS on option {option} (step {step})", file=sys.stderr)
            port.tap("cross", 8)
        # Watch at the card machine's own resolution until the page leaves the menu.
        for _ in range(FAIRY_MENU_WATCH_STEPS // FAIRY_WATCH_STEPS):
            port.run(FAIRY_WATCH_STEPS)
            if port.word(G_FAIRY_PAGE) != 0:
                return port.word(G_FAIRY_PAGE), option
    page = port.word(G_FAIRY_PAGE)
    if page == 0:
        raise ToolError(
            f"the fairy's menu stayed on page 0 with m_MenuSelectedOption {option} after "
            f"{FAIRY_MENU_STEPS} press(es) watched for {FAIRY_MENU_WATCH_STEPS} step(s) each: DOWN moves the "
            f"option towards {FAIRY_MENU_OPTION} and CROSS takes it, and the guest took neither"
        )
    return page, option


def _await_write(port: ScenePort, page_before: int, option: int, has_card: int) -> Proof:
    """The proof that the page sequence entered 2 and ended on 7, or a refusal naming where it stopped."""
    seen: list[int] = []
    spent = 0
    while spent < FAIRY_CARD_STEPS:
        page = port.word(G_FAIRY_PAGE)
        if not seen or seen[-1] != page:
            seen.append(page)
            print(f"save: m_MenuDialoguePage {page} at step {port.spent()}", file=sys.stderr)
        if page == SAVE_PAGE_SAVED:
            break
        if page in range(SAVE_PAGE_FAILED_FIRST, SAVE_PAGE_FAILED_FIRST + 4):
            break
        if page == 0:
            break
        port.run(FAIRY_WATCH_STEPS)
        spent += FAIRY_WATCH_STEPS
    walked = " -> ".join(dict.fromkeys(
        [str(page_before), str(SAVE_PAGE_WRITE)] + [str(page) for page in seen]))
    if SAVE_PAGE_WRITE not in seen:
        raise ToolError(
            f"the fairy's page machine walked {' -> '.join(str(page) for page in seen)} and never "
            f"entered page {SAVE_PAGE_WRITE}, "
            f"the only page SaveCreate and MemCardWriteFile are reached from (it was on page "
            f"{page_before} when CROSS was pressed, with option {option} and "
            f"m_HasMemoryCard={has_card}). Pages {SAVE_PAGE_FAILED_FIRST}-"
            f"{SAVE_PAGE_FAILED_FIRST + 3} are the guest's own failure pages and page 0 is the "
            "menu, so this is the route not getting a card write and naming where it stopped"
        )
    if port.word(G_FAIRY_PAGE) != SAVE_PAGE_SAVED:
        raise ToolError(
            f"m_MenuDialoguePage walked {walked}: the write page {SAVE_PAGE_WRITE} was entered and "
            f"the guest ended on {port.word(G_FAIRY_PAGE)} after {port.spent()} step(s) instead "
            f"of page {SAVE_PAGE_SAVED} ('GAME SAVED'), so the guest did not report the save"
        )
    return Proof(
        "save-fairy",
        f"GS_Fairy with g_FairyCutscene.m_MenuDialoguePage {walked}, page "
        f"{SAVE_PAGE_WRITE} being the only one SaveCreate (0x800321F4) and MemCardWriteFile "
        f"(0x80032230) are reached from and page {SAVE_PAGE_SAVED} the one the guest captions "
        f"'GAME SAVED', the card machine's stage word reaching {port.word(G_FAIRY_CARD_STAGE)}",
        port.spent(),
        {
            "g_Gamestate": port.word(guest_globals.kGamestate),
            "g_FairyCutscene.m_State": port.word(G_FAIRY_STATE),
            "m_MenuSelectedOption": option,
            "m_HasMemoryCard": has_card,
            "m_MenuDialoguePage": port.word(G_FAIRY_PAGE),
            "pages_walked": walked,
            "card_stage_at_end": port.word(G_FAIRY_CARD_STAGE),
            "g_LevelId": port.word(guest_globals.kLevelId),
        },
    )


def _await(port: ScenePort, predicate: Callable[[], bool], what: str, budget: int) -> bool:
    """Advance in small steps until `predicate` holds or the budget runs out."""
    spent = 0
    while spent < budget:
        if predicate():
            return True
        port.run(5)
        spent += 5
    return predicate()


@dataclass(frozen=True)
class Scene:
    name: str
    description: str
    run: Callable[[ScenePort], Proof]


SCENES = {
    scene.name: scene
    for scene in (
        Scene("flight-level", "a flight level (Sunny Flight)", flight_level),
        Scene("boss-level", "a boss fight (Toasty)", boss_level),
        Scene("death-respawn", "a player death and respawn", death_respawn),
        Scene("save-fairy", "the in-game save (the fairy's card write)", save_fairy),
    )
}


def run_scene(name: str, port) -> Proof:
    """Run one named scene against a live product and return what it proved."""
    if name not in SCENES:
        raise ToolError(f"unknown scene {name!r}; scenes: {', '.join(sorted(SCENES))}")
    return SCENES[name].run(ScenePort(port, port.frame))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--list", action="store_true", dest="list_scenes")
    parser.parse_args()
    for scene in SCENES.values():
        print(f"{scene.name:16} {scene.description}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
