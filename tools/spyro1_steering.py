#!/usr/bin/env python3
"""Which way to press to walk at a place, and where this level's places are.

Shared by the product driver and the oracle comparison so both cores take the same route. Every
reader takes a `words(address, count) -> list[int]` callable; nothing here writes guest memory.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable

import guest_globals
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

Words = Callable[[int, int], list[int]]

# Record layouts. The addresses are the shipping owner's; these offsets are read only here.
MOBY_BYTES = 0x58
MOBY_CLASS_WORD = 52   # the class is the high half of this word
MOBY_STATE = 72
MOBY_POSITION = 12     # three signed words
MAX_LEVEL_MOBYS = 1024
PORTAL_CENTER = 0x20   # Portal::m_Center, after the skybox pointer, counts and world sector
PORTAL_LEVEL = 0x1C
PORTAL_SLOTS = 6
CAMERA_WORDS = 14      # 5 packed matrix words, filler, then the position at +0x28

RAM_BEGIN, RAM_END = 0x80000000, 0x80200000

# 0 deg is straight ahead, +90 screen-right; each entry is a 45-degree sector's upper bound and its centre direction.
SECTORS = (
    (-157.5, ("down",)),
    (-112.5, ("down", "left")),
    (-67.5, ("left",)),
    (-22.5, ("up", "left")),
    (22.5, ("up",)),
    (67.5, ("up", "right")),
    (112.5, ("right",)),
    (157.5, ("down", "right")),
)


def signed(value: int) -> int:
    return value - 0x100000000 if value >= 0x80000000 else value


@dataclass(frozen=True)
class Target:
    """A place worth walking to, and what named thing it is."""

    what: str
    position: tuple[int, int, int]


@dataclass(frozen=True)
class View:
    """The camera as the renderer holds it: a 3x3 fixed-point matrix and a world position."""

    matrix: tuple[tuple[int, int, int], ...]
    position: tuple[int, int, int]

    def bearing(self, target: tuple[int, int, int]) -> tuple[int, float]:
        """Ground distance to `target` in view-space units, and its bearing in degrees."""
        view = self._view(target)
        return int(math.hypot(view[0], view[2])), math.degrees(math.atan2(view[0], view[2]))

    def nearest(self, targets) -> tuple[int, int, float]:
        """Index, distance and bearing of the closest of `targets`; refuses an empty set."""
        if not targets:
            raise ToolError("nothing to steer at: the target list is empty")
        best = None
        for index, target in enumerate(targets):
            distance, bearing = self.bearing(target.position)
            if best is None or distance < best[1]:
                best = (index, distance, bearing)
        return best

    def _view(self, position) -> list[int]:
        # The renderer's own packing: X is target-minus-camera, Y and Z are camera-minus-target, all
        # arithmetic-shifted down two, and the matrix rows consume them in Y/Z/X order.
        relative = (
            (position[0] - self.position[0]) >> 2,
            (self.position[1] - position[1]) >> 2,
            (self.position[2] - position[2]) >> 2,
        )
        source = (relative[1], relative[2], relative[0])
        return [sum(self.matrix[row][i] * source[i] for i in range(3)) >> 12 for row in range(3)]


def sector(bearing: float) -> tuple[str, ...]:
    """The pad direction whose compass sector contains `bearing`."""
    for limit, buttons in SECTORS:
        if bearing <= limit:
            return buttons
    return ("down",)  # past +157.5 the compass wraps back into the first sector


def camera(words: Words) -> View:
    """The camera's matrix and position, read the way the renderer packs them."""
    observed = words(guest_globals.kCamera, CAMERA_WORDS)

    def half(value: int, high: bool) -> int:
        part = (value >> 16) & 0xFFFF if high else value & 0xFFFF
        return part - 0x10000 if part >= 0x8000 else part

    matrix = (
        (half(observed[0], False), half(observed[0], True), half(observed[1], False)),
        (half(observed[1], True), half(observed[2], False), half(observed[2], True)),
        (half(observed[3], False), half(observed[3], True), half(observed[4], False)),
    )
    return View(matrix, tuple(signed(observed[10 + i]) for i in range(3)))


def player_view(words: Words) -> View:
    """The camera's matrix placed at Spyro's position: the pad is camera-relative, distances are his."""
    view = camera(words)
    position = tuple(signed(v) for v in words(guest_globals.kSpyro, 3))
    return View(view.matrix, position)


def portal_targets(words: Words, level: int | None = None) -> list[Target]:
    """Centres of the live entries of g_Portals, optionally only those leading to `level`."""
    count = words(guest_globals.kPortalCount, 1)[0]
    if not 0 <= count <= PORTAL_SLOTS:
        raise ToolError(f"g_PortalCount holds {count}, which is outside the table's {PORTAL_SLOTS} slots")
    found: list[Target] = []
    for index in range(count):
        portal = words(guest_globals.kPortals + index * 4, 1)[0]
        if not RAM_BEGIN <= portal < RAM_END:
            raise ToolError(
                f"g_Portals[{index}] holds 0x{portal:08X}, which is not a RAM address, so the "
                "portal table was read before the level finished loading"
            )
        destination = signed(words(portal + PORTAL_LEVEL, 1)[0])
        center = tuple(signed(v) for v in words(portal + PORTAL_CENTER, 3))
        if level is None or destination == level:
            found.append(Target(f"portal to level {destination}", center))
    return found


def moby_class_targets(words: Words, wanted_class: int) -> list[Target]:
    """World positions of every live Moby of a class. Read once: level Mobys do not walk away."""
    first = words(guest_globals.kLevelMobys, 1)[0]
    if not RAM_BEGIN <= first < RAM_END:
        raise ToolError(f"g_LevelMobys holds 0x{first:08X}, which is not a RAM address")
    seen: list[int] = []
    found: list[Target] = []
    index = 0
    while index < MAX_LEVEL_MOBYS:
        need = (index + 1) * MOBY_BYTES // 4 + 1
        while len(seen) < need:
            seen.extend(words(first + len(seen) * 4, 64))
        base = index * MOBY_BYTES // 4
        state = seen[base + MOBY_STATE // 4]
        if state & 0x80:
            if (state & 0xFF) == 0xFF:
                break
            index += 1
            continue
        if (seen[base + MOBY_CLASS_WORD // 4] >> 16) & 0xFFFF == wanted_class:
            found.append(Target(f"class {wanted_class}",
                                tuple(signed(seen[base + MOBY_POSITION // 4 + i]) for i in range(3))))
        index += 1
    return found


@dataclass(frozen=True)
class Decision:
    """What to press for the next stretch of walking, and why."""

    buttons: tuple[str, ...]
    hop: bool
    distance: int
    bearing: float
    detour: int

    def describe(self) -> str:
        return (f"{self.distance} away, bearing {self.bearing:+.0f}"
                f"{f' detour {self.detour:+d}' if self.detour else ''}"
                f"{' (hopping)' if self.hop else ''} -> {'+'.join(self.buttons)}")


class Walk:
    """The route policy: close the gap, detour around what blocks it, hop onto what it stands on,
    and abandon a target that never gets closer. It takes no steps, so any driver can use it."""

    ARRIVED = 500       # view-space units (~1/4 of world scale); close enough to be on screen
    STALL_STEPS = 15    # decisions without closing the gap before this target is abandoned
    PROGRESS = 200      # view-space units that count as having closed the gap
    # Decisions of no progress before the walk starts hopping onto what it stands against.
    JUMP_AFTER = 3
    # Bearing offsets tried in turn once the straight line stops closing the gap.
    DETOURS = (0, 60, -60, 120, -120)

    def __init__(self, what: str, targets: list[Target], arrived: int | None = None,
                 step_scale: float = 1.0, detour: bool = True):
        if not targets:
            raise ToolError(
                f"nothing to walk at: this level carries no {what}, so the destination is wrong or "
                "this is not the level that carries it"
            )
        self.what = what
        # A recorded route already goes around obstacles, so it walks without detours.
        self.detours = self.DETOURS if detour else (0,)
        # The thresholds are tuned for a 24-field decision; `step_scale` keeps distance and fields equal.
        self.progress = self.PROGRESS * step_scale
        self.stall_steps = max(1, round(self.STALL_STEPS / step_scale))
        self.jump_after = max(1, round(self.JUMP_AFTER / step_scale))
        # arrived=0 walks into the destination until the driver's own predicate fires.
        self.arrived = self.ARRIVED if arrived is None else arrived
        self.remaining = list(targets)
        self.abandoned: list[str] = []
        self.best: int | None = None
        self.closest: int | None = None
        self.stalled = 0
        self.decisions = 0

    def next(self, view: View) -> Decision | None:
        """The next decision, or None once the nearest target is within ARRIVED."""
        while True:
            if not self.remaining:
                raise ToolError(
                    f"every {self.what} is unreachable from here: {'; '.join(self.abandoned)}. The "
                    "route needs a different entry, not more walking"
                )
            index, distance, bearing = view.nearest(self.remaining)
            self.closest = distance
            if distance <= self.arrived:
                return None
            if self.best is None or distance < self.best - self.progress:
                self.best, self.stalled = distance, 0
                break
            self.stalled += 1
            if self.stalled < self.stall_steps:
                break
            # Blocked: take the next nearest destination.
            self.abandoned.append(f"{self.remaining[index].what} stuck at {distance}")
            self.remaining.pop(index)
            self.best, self.stalled = None, 0
        detour = self.detours[self.stalled % len(self.detours)] if self.stalled else 0
        steered = (bearing + detour + 180.0) % 360.0 - 180.0
        self.decisions += 1
        return Decision(sector(steered), self.stalled >= self.jump_after, distance, bearing, detour)

    def rebase(self) -> None:
        """Forget the progress made so far, after a cutscene moved Spyro."""
        self.best, self.stalled = None, 0

    def exhausted(self, steps: int) -> ToolError:
        """The refusal a driver raises when its own budget runs out, with the distance reached."""
        return ToolError(
            f"the nearest {self.what} was still about {self.closest} away after {steps} steering "
            f"step(s) and {self.decisions} decision(s)"
        )


