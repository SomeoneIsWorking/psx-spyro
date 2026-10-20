#!/usr/bin/env python3
"""Spyro 1's attract-demo oracle route: no pad input, through the title timeout into the level-11 demo.

The title times out and plays a recorded input stream (titlescreen.c:179-210, gamepad.c:165-180), so both
cores reach the level entry without steering (docs/issues/0114, 0132). During playback the guest owns
the pad, so the pad ranges only discriminate input delivery in the title phase.
"""

from __future__ import annotations

import guest_globals
import oracle_spyro1 as base
from compare import Checkpoint, CoreSession, DeclaredRange, Driver, Settle, u32
from title_states import GS_PLAYING, GS_TITLE_SCREEN, TSM_DEMO

name = f"{base.name} — attract demo"

# g_DemoMode (gamepad.h:41-43), the end-of-demo counter (update.c:928-950), m_DemoType at title +0x1C.
G_DEMO_MODE = 0x80075714
G_DEMO_FADE_TIMER = 0x80075884
G_TITLE_DEMO_TYPE = guest_globals.kTitlescreenState + 0x1C

TSS_SETUP, TSS_LOADING = 0, 1   # include/titlescreen.h:58-60
DEMO_TYPE_DEMO_LEVEL = 2        # TSD_DemoLevel: load g_DemoLevelIds[g_DemoIndex]
DEMO_LEVEL_FIRST = 0x0B         # g_DemoLevelIds[0] (asm/data/math.data.s:2751), the fresh-boot demo
DEMO_PLAYBACK_TICKS = 0x35C - 8  # g_DemoLengths[0] - 8 (asm/data/math.data.s:2758), update.c:932
DEMO_FADE_TICKS = 16            # g_DemoFadeTimer counts to 16 and the title screen comes back (update.c:950)

# Both decisive: g_DemoMode picks the pad source and the fade counter ticks once per demo update.
declared = base.declared + (
    DeclaredRange("demo.mode", G_DEMO_MODE, 4, True),
    DeclaredRange("demo.fade_timer", G_DEMO_FADE_TIMER, 4, True),
)

excluded = base.excluded
picture_decisive = base.picture_decisive
observe = base.observe
lookahead = base.lookahead
summary = base.summary
Observation = base.Observation


# One field per advance, except during demo playback, where g_UnprocessedFrames never moves
# (gamepad.c:214, main.c:24-32): there wait for g_GameTick, the first statement of the GS_Playing
# update (update.c:1094), so both cores park after the same number of updates (docs/issues/0132).
DEMO_MODE_PLAY = 1  # include/gamepad.h:41-43


def _in_demo_playback(core: CoreSession) -> bool:
    return (u32(core, G_DEMO_MODE) == DEMO_MODE_PLAY
            and u32(core, guest_globals.kGamestate) == GS_PLAYING)


def advance(core: CoreSession, frames: int, strict: bool = False) -> None:
    for _ in range(frames):
        if _in_demo_playback(core):
            previous = u32(core, guest_globals.kGameTick)
            while u32(core, guest_globals.kGameTick) == previous:
                core.step(1)
        else:
            core.step(1)


# --budget counts advance steps, here fields; the reference reaches the first checkpoint near field 4766.
FIELD_BUDGET = 8000


def _field_budget(budget: int) -> int:
    if budget >= FIELD_BUDGET:
        return budget
    print(f"[oracle] demo route: --budget {budget} counts fields here and cannot reach the first "
          f"checkpoint; using {FIELD_BUDGET}.")
    return FIELD_BUDGET


def no_input(frame: int, seen: Observation) -> frozenset[str]:
    """No input: one Start or Cross at TSM_Init sub-state 3 would cancel the timeout (titlescreen.c:176-181)."""
    return frozenset()


def _demo_owns_title(seen: Observation) -> bool:
    """The title has handed off to the demo and its level load is under way (update.c:629-694)."""
    return (seen.gamestate == GS_TITLE_SCREEN
            and seen.title.mode == TSM_DEMO
            and seen.title.state >= TSS_LOADING)


def _demo_playing(seen: Observation) -> bool:
    """The demo's own level is running an update past its load-time tick reset (loaders.c:401-402)."""
    return (seen.gamestate == GS_PLAYING
            and seen.level == DEMO_LEVEL_FIRST
            and seen.game_tick >= 1)


# Predicates name states a core parks at; neither advance clause can step over one.
def reach_demo_level_load(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    return driver.drive(core, _field_budget(budget), _demo_owns_title, no_input, "attract demo level load", settle)


def reach_demo_playing(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    return driver.drive(core, _field_budget(budget), _demo_playing, no_input, "attract demo gameplay", settle)


checkpoints = (
    Checkpoint("demo_level_load", reach_demo_level_load),
    # level_id and load_stage are decisive, so a wrong level fails here.
    Checkpoint("demo_playing", reach_demo_playing),
)

# Main-loop iterations inside the playback, which ends at g_DemoLengths[0] - 8 (update.c:932), with nothing held.
gameplay = (
    (frozenset(), 400),
    (frozenset(), 300),
)
