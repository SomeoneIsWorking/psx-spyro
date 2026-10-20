#!/usr/bin/env python3
"""Walk Spyro to a place by pressing the pad over the port's REPL.

`spyro1_steering.Walk` decides where to press; this module presses, runs and releases.
"""

from __future__ import annotations

import sys
from typing import Callable, Protocol

import guest_globals
import spyro1_steering
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from spyro1_steering import Walk

# common.h's Gamestate values the walker acts on; every other state is waited out.
GS_PLAYING = 0
GS_DRAGON = 8  # the "Press Cross" dragon cutscene that opens a homeworld; it takes Cross to leave

Target = spyro1_steering.Target


class ReplPort(Protocol):
    """The slice of tools/drive.py's Port this module drives."""

    def press(self, button: str) -> None: ...
    def tap(self, button: str, frames: int = ...) -> None: ...
    def run(self, frames: int) -> int: ...
    def words(self, address: int, count: int = ...) -> list[int]: ...


class Seeker:
    """Walk the product to one of a set of world positions, steering from the guest's own camera.

    `stop` ends the walk when something happens at the destination; `hop_within` jumps on every
    decision inside that view-space distance (the fairy's trigger has a height window).
    """

    # Product steps the guest may spend outside GS_Playing; the opening dragon alone is ~1600 fields.
    CUTSCENE_BUDGET = 3000
    JUMP_PRESS = 2  # steps Cross is held before it is released
    JUMP_SETTLE = 2  # steps with nothing held between the release and the direction
    JUMP_AIRTIME = 30  # steps during which a jump is not started again
    STEP = 12  # steps per steering decision; a whole walk cycle, short enough to correct a wall

    def __init__(self, port: ReplPort, what: str, targets, budget: int = 6000,
                 arrived: int | None = None, stop: Callable[[], bool] | None = None,
                 stop_is: str = "", step: int | None = None,
                 from_player: bool = False, hop_within: int = 0, hop: bool = True,
                 detour: bool = True):
        self._port = port
        self._budget = budget
        self._stop = stop
        self._stop_is = stop_is or "the destination"
        self._waited = 0
        self._jump_until = 0
        self._step = self.STEP if step is None else step
        self._walk = Walk(what, list(targets), arrived=arrived, step_scale=self._step / self.STEP,
                          detour=detour)
        self._hop_within = hop_within
        self._hop = hop
        self._held: set[str] = set()
        self._view = spyro1_steering.player_view if from_player else spyro1_steering.camera

    def _cutscene_step(self) -> bool:
        """Advance one step outside GS_Playing without counting it as a stall; Cross leaves the dragon."""
        state = self._port.words(guest_globals.kGamestate, 1)[0]
        if state == GS_PLAYING:
            return False
        if self._stop is not None and self._stop():
            return False
        self._walk.rebase()
        self._hold(set())
        self._waited += self._step
        if self._waited > self.CUTSCENE_BUDGET:
            raise ToolError(
                f"the guest stayed in gamestate {state} for {self._waited} step(s) without "
                "returning to GS_Playing, so the pad is not steering anything"
            )
        print(f"seek: gamestate {state}, not steering", file=sys.stderr)
        if state == GS_DRAGON:
            self._port.tap("cross", 8)
        self._port.run(self._step)
        return True

    def _hold(self, wanted: set[str]) -> None:
        """Make exactly `wanted` the buttons held: press what is new, release what is stale."""
        for button in self._held - wanted:
            self._port.release(button)
        for button in wanted - self._held:
            self._port.press(button)
        self._held = wanted

    def walk(self) -> int:
        """The view-space distance actually reached, or refuse by name; the pad is always released."""
        try:
            return self._walk_loop()
        finally:
            self._hold(set())

    def _walk_loop(self) -> int:
        for target in self._walk.remaining:
            print(f"seek: {target.what} at {target.position}", file=sys.stderr)
        spent = 0
        while spent < self._budget:
            if self._cutscene_step():
                continue
            if self._stop is not None and self._stop():
                print(f"seek: {self._stop_is} after {spent} step(s), "
                      f"{self._walk.closest} from the nearest {self._walk.what}", file=sys.stderr)
                return self._walk.closest or 0
            decision = self._walk.next(self._view(self._port.words))
            if decision is None:
                print(f"seek: reached {self._walk.what} at {self._walk.closest}", file=sys.stderr)
                return self._walk.closest
            print(f"seek: {decision.describe()}", file=sys.stderr)
            # A jump is Cross pressed and released, then the direction; held Cross never leaves the ground.
            jump = (decision.hop and self._hop) or decision.distance <= self._hop_within
            airborne = spent < self._jump_until
            if jump and not airborne:
                self._hold({"cross"})
                self._port.run(self.JUMP_PRESS)
                self._hold(set())
                self._port.run(self.JUMP_SETTLE)
                spent += self.JUMP_PRESS + self.JUMP_SETTLE
                self._jump_until = spent + self.JUMP_AIRTIME
            self._hold(set(decision.buttons))
            self._port.run(self._step)
            spent += self._step
        self._hold(set())
        if self._stop is not None:
            raise ToolError(
                f"walked to within {self._walk.closest} of a {self._walk.what} over {spent} "
                f"step(s), but {self._stop_is} never happened: being next to one is not entering it"
            )
        raise self._walk.exhausted(spent)
