#!/usr/bin/env python3
"""Follow a recorded waypoint route (tools/routes/*.json) through a level, one Seeker per leg.

A route is refused outside the level it was recorded in; a leg ends when Spyro is within LEG_ARRIVED.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Callable

import guest_globals
import repl_walk
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from spyro1_steering import Target

ROUTES_DIR = Path(__file__).resolve().parent / "routes"
LEG_ARRIVED = 200    # view-space units (~800 world, measured from Spyro): a waypoint is a way
LEG_BUDGET = 1500    # fields one leg may take before the route refuses
# Routes decide every 8 fields: Spyro covers ~1000 units in the seeker's default 24-field decision.
ROUTE_STEP = 8
CLIMB_ALWAYS = 1 << 30  # a hop_within no distance exceeds: hop on every decision


class Route:
    """One recorded route: the level it belongs to and its world-space waypoints."""

    def __init__(self, level: int, name: str, waypoints: list[tuple[int, ...]]):
        if not waypoints:
            raise ToolError(f"route {name!r} has no waypoints")
        self.level = level
        self.name = name
        self.waypoints = waypoints

    @classmethod
    def load(cls, path: Path, name: str) -> "Route | None":
        """The named route, or None when the file has none; a malformed file refuses."""
        data = json.loads(path.read_text())
        raw = data["routes"].get(name)
        if raw is None:
            return None
        points = []
        for point in raw:
            if len(point) not in (3, 4) or not all(isinstance(v, int) for v in point):
                raise ToolError(f"route {name!r} in {path.name} has a malformed waypoint {point!r}")
            points.append(tuple(point))
        return cls(int(data["level"]), name, points)

    @classmethod
    def find(cls, level: int, name: str) -> "Route | None":
        """The recorded route `name` for `level`, searching every file under tools/routes."""
        for path in sorted(ROUTES_DIR.glob("*.json")):
            if json.loads(path.read_text())["level"] != level:
                continue
            route = cls.load(path, name)
            if route is not None:
                return route
        return None


def follow(port, route: Route, stop: Callable[[], bool] | None = None, stop_is: str = "") -> None:
    """Walk every waypoint of `route` in order, refusing in another level or on a leg that never
    arrives; `stop` ends the route once what it exists to cause has happened."""
    level = port.words(guest_globals.kLevelId, 1)[0]
    if level != route.level:
        raise ToolError(
            f"route {route.name!r} was recorded in level {route.level} but g_LevelId is {level}"
        )
    for index, point in enumerate(route.waypoints):
        if stop is not None and stop():
            return
        what = f"waypoint {index + 1}/{len(route.waypoints)} of {route.name}"
        print(f"route: {what} at {point}", file=sys.stderr)
        # A fourth element of 1 marks a leg that climbs a wall: it jumps on every decision.
        climbing = len(point) == 4 and point[3] == 1
        repl_walk.Seeker(port, what, [Target(what, tuple(point[:3]))], budget=LEG_BUDGET,
                         arrived=LEG_ARRIVED, step=ROUTE_STEP, from_player=True,
                         hop_within=CLIMB_ALWAYS if climbing else 0, hop=False,
                         detour=False, stop=stop, stop_is=stop_is).walk()
