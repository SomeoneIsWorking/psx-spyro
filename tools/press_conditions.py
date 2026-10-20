#!/usr/bin/env python3
"""A pad edge a driven run issues the first time the guest is observed in a named gamestate.

`drive.py --press-while` arms pre-arrival presses, applied by the navigator before gameplay;
`--press-after` arms post-arrival presses, applied by `Port.run()`'s sampler for the whole run and
refused for states where a button is gameplay or menu input. Each condition fires once, ever: a
re-armed `0:start` fired again at the GS_Playing hand-off and opened the pause menu.
"""

from __future__ import annotations

from dataclasses import dataclass

import title_identity  # noqa: F401  (puts the framework tools on sys.path)
from automation.process import ToolError  # noqa: E402
from title_states import GAMESTATE_NAMES, GS_INVENTORY_MENU, GS_PAUSE_MENU, GS_PLAYING

# A shorter edge can be missed by a title owner that samples input every other field.
EDGE_FRAMES = 8

POST_ARRIVAL_FORBIDDEN: dict[int, str] = {
    GS_PLAYING: "a pad button there is gameplay input, and Start opens the pause menu",
    GS_PAUSE_MENU: "Start and Cross there are menu commands (confirm, resume), not a skip",
    GS_INVENTORY_MENU: "Start and Cross there are menu commands (close, back)",
}


@dataclass
class PressCondition:
    """One button, fired on the first sample that finds the guest in `gamestate`."""

    gamestate: int
    button: str
    frames: int = EDGE_FRAMES
    fired: bool = False
    samples_in_state: int = 0
    times_fired: int = 0

    def name(self) -> str:
        return f"{self.button} while in {GAMESTATE_NAMES.get(self.gamestate, self.gamestate)}"

    def observe(self, gamestate: int) -> bool:
        """Record one sample; True when this sample is the one that owes an edge."""
        if gamestate != self.gamestate:
            return False
        self.samples_in_state += 1
        if self.fired:
            return False
        self.fired = True
        return True


class PressConditions:
    """A set of condition-triggered pad edges for one phase of a run."""

    def __init__(self, conditions: list[PressCondition] | None = None, phase: str = "pre-arrival"):
        self.conditions = list(conditions or ())
        self.phase = phase

    def apply(self, port, gamestate: int) -> list[PressCondition]:
        """Fire every edge owed for the gamestate the caller just sampled; `port` needs only `tap`."""
        due = [c for c in self.conditions if c.observe(gamestate)]
        for condition in due:
            port.tap(condition.button, condition.frames)
            condition.times_fired += 1
        return due

    def report(self) -> str:
        """Samples seen and edges issued per condition, naming the ones that never matched."""
        if not self.conditions:
            return f"{self.phase} presses: none requested"
        parts = []
        for condition in self.conditions:
            part = (f"{condition.name()} fired {condition.times_fired} time(s) "
                    f"in {condition.samples_in_state} sample(s)")
            if not condition.times_fired:
                part += " -- scanned, never matched"
            parts.append(part)
        return f"{self.phase} presses: " + "; ".join(parts)


def parse(spec: str) -> PressCondition:
    """`GAMESTATE:BUTTON[:FRAMES]` -> a condition; every malformed form is refused by name."""
    fields = spec.split(":")
    if len(fields) not in (2, 3) or not fields[0].strip() or not fields[1].strip():
        raise ToolError(
            f"press spec {spec!r} is not GAMESTATE:BUTTON[:FRAMES]; "
            f"a gamestate is a number ({', '.join(f'{s}={n}' for s, n in sorted(GAMESTATE_NAMES.items()))})"
        )
    try:
        gamestate = int(fields[0], 0)
    except ValueError:
        raise ToolError(f"press spec {spec!r} names a gamestate that is not a number: {fields[0]!r}") from None
    frames = EDGE_FRAMES
    if len(fields) == 3:
        try:
            frames = int(fields[2], 0)
        except ValueError:
            raise ToolError(f"press spec {spec!r} names a frame count that is not a number: {fields[2]!r}") from None
        if frames < 1:
            raise ToolError(f"press spec {spec!r} asks for {frames} frames; an edge needs at least one")
    return PressCondition(gamestate=gamestate, button=fields[1].strip(), frames=frames)


def parse_post(spec: str) -> PressCondition:
    """A post-arrival `GAMESTATE:BUTTON[:FRAMES]`, refusing a state where a button is not a skip."""
    condition = parse(spec)
    reason = POST_ARRIVAL_FORBIDDEN.get(condition.gamestate)
    if reason is not None:
        raise ToolError(
            f"--press-after {spec!r} names {GAMESTATE_NAMES.get(condition.gamestate, condition.gamestate)}: {reason}"
        )
    return condition


def parse_all(specs: list[str]) -> PressConditions:
    return PressConditions([parse(spec) for spec in specs])


def parse_post_all(specs: list[str]) -> PressConditions:
    return PressConditions([parse_post(spec) for spec in specs], "post-arrival")
