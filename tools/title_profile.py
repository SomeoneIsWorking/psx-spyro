#!/usr/bin/env python3
"""The per-title facts a driver needs to play Spyro 2 or Spyro 3.

Game-state and level-id words come from each title's `core/boot_facts.h` and Spyro 3's camera
position from its `render/render_facts.h`; the rest are cited inline.

    uv run --frozen python tools/title_profile.py
"""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass

import guest_globals
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

_SPYRO2 = guest_globals.title_facts("spyro2")
_SPYRO3 = guest_globals.title_facts("spyro3")


@dataclass(frozen=True)
class LevelFacts:
    """Where a title keeps its level and the guest's name table, indexed `(homeworld << 5) + level`."""

    homeworld_word: int
    level_word: int
    level_id_word: int
    name_table: int

    def name_pointer_address(self, homeworld: int, level: int) -> int:
        return self.name_table + 4 * ((homeworld << 5) + level)


@dataclass(frozen=True)
class TitleProfile:
    label: str
    slug: str
    gamestate_word: int
    state_title: int
    state_loading: int
    state_playing: int
    # The first word of an (x, y, z) triple that follows the player; y is +4 and z +8.
    follow_word: int
    # Walks away from the first conversation.
    walk_button: str
    # None when this title's conversation has not been observed.
    state_dialogue: int | None = None
    dialogue_button: str | None = None
    level: LevelFacts | None = None

    def __post_init__(self) -> None:
        states = {self.state_title, self.state_loading, self.state_playing}
        if len(states) != 3:
            raise ToolError(f"{self.label}: title, loading and playing must be three distinct game states")

    def follow_words(self) -> tuple[int, int, int]:
        return self.follow_word, self.follow_word + 4, self.follow_word + 8


TITLES: Mapping[str, TitleProfile] = {
    "spyro2": TitleProfile(
        label="Spyro 2 (SCUS_944.25)",
        slug="spyro2",
        gamestate_word=_SPYRO2["kGameState"],
        state_title=11,
        state_loading=5,
        state_playing=0,
        # The camera's copy of the player position: FUN_80016328 copies 0x80069FF0 here each field.
        follow_word=0x80067EE4,
        walk_button="down",
        state_dialogue=1,
        dialogue_button="up",
        # 0x80014450..0x8001447C index the name table at 0x800649B4 by these two words.
        level=LevelFacts(
            homeworld_word=0x80067118,
            level_word=0x80066F54,
            level_id_word=_SPYRO2["kLevelId"],
            name_table=0x800649B4,
        ),
    ),
    "spyro3": TitleProfile(
        label="Spyro 3 (SCUS_944.67)",
        slug="spyro3",
        gamestate_word=_SPYRO3["kGameState"],
        state_title=11,
        state_loading=5,
        state_playing=0,
        follow_word=guest_globals.render_field("spyro3", "cameraPosition"),
        walk_button="up",
    ),
}


def profile(name: str) -> TitleProfile:
    try:
        return TITLES[name]
    except KeyError:
        raise ToolError(f"unknown title {name!r}; the profiles are {', '.join(sorted(TITLES))}") from None


if __name__ == "__main__":
    for key, entry in TITLES.items():
        print(f"{key}: {entry}")
