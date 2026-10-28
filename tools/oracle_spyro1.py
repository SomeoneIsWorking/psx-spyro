#!/usr/bin/env python3
"""Spyro 1's side of the oracle comparison (psxport tools/oracle/compare.py, docs/oracle.md).

Every predicate reads main RAM only: the console reference cannot read the scratchpad.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

import guest_globals
import title_prompts

from compare import (
    Checkpoint,
    CoreSession,
    DeclaredRange,
    Driver,
    Settle,
    step_until_counter_resets,
    u32,
)
from title_states import (
    GS_PLAYING,
    GS_TITLE_SCREEN,
    TSM_LOADING,
    TitleState,
)

name = "Spyro the Dragon (SCUS_942.28)"

# The collision query's result for the camera group.
D_OCCLUSION_RESULT = 0x80075844

declared = (
    # Informational: 10 of these bytes differ from the console at every checkpoint (issue 0125).
    DeclaredRange("kernel_page0", 0x80000000, 0x80, False),
    DeclaredRange("gamestate", guest_globals.kGamestate, 4, True),
    DeclaredRange("title.mode_state", guest_globals.kTitlescreenState, 8, True),       # m_Mode, m_State
    DeclaredRange("title.ticks", guest_globals.kTitlescreenState + 8, 8, False),        # m_Tick, m_SubTick
    DeclaredRange("title.sub_state_option", guest_globals.kTitlescreenState + 16, 8, True),  # m_SubState, m_OptionSelected
    DeclaredRange("load_stage", guest_globals.kLoadStage, 4, True),
    DeclaredRange("level_id", guest_globals.kLevelId, 4, True),
    DeclaredRange("game_tick", guest_globals.kGameTick, 4, True),
    DeclaredRange("level_ticks", guest_globals.kLevelTicks, 4, False),
    DeclaredRange("delta_time", guest_globals.kDeltaTime, 4, False),
    DeclaredRange("state_switch", guest_globals.kStateSwitch, 4, True),
    DeclaredRange("player.position", guest_globals.kSpyro, 12, True),
    DeclaredRange("player.state", guest_globals.kPlayerState, 4, True),
    DeclaredRange("player", guest_globals.kSpyro, 0x2A8, False),
    DeclaredRange("camera", guest_globals.kCamera, 0x110, False),
    # The rescue cutscene's streaming state machine, m_CutsceneIdx through m_Fade.
    DeclaredRange("dragon_cutscene", guest_globals.kDragonCutscene + 0x24, 0x24, False),
    # The camera's occlusion group from the collision query at 0x8004DF24.
    DeclaredRange("occlusion_result", D_OCCLUSION_RESULT, 4, True),
    # The VBlank poll's words. Informational: the product parks after the polls its next update reads and the
    # console before them (see lookahead), so on a frame where the input changes they differ by one poll.
    DeclaredRange("pad.down", guest_globals.kPad, 4, False),
    DeclaredRange("pad.released", guest_globals.kPad + 4, 4, False),
    DeclaredRange("pad.held", guest_globals.kPad + 8, 4, False),
    DeclaredRange("pad.poll", guest_globals.kPad + 0xC, 0xA4, False),
    # The sound engine's 24 voice rows and its pending key-on/key-off masks (0x80056ED4 flushes them).
    DeclaredRange("sound.voices", 0x80075F30, 0x2A0, False),
    DeclaredRange("sound.pending_keys", 0x80076238, 8, False),
)

# Also decisive for the picture: the camera and the dragon cutscene's fade move every pixel (issues 0110, 0114).
picture_decisive = tuple(r.name for r in declared if r.decisive) + ("camera", "dragon_cutscene")

excluded = {
    "g_LevelTicks (informational only)":
        "a draw-less iteration's field count follows retail's VSync phase, which the host clock cannot "
        "reproduce, so the counter keeps a small offset from each load (docs/issues/0110)",
    "g_DeltaTime, g_Camera and the dragon cutscene's tick (informational only)":
        "read from the counter above, so they carry its offset; the camera's yaw residual makes a "
        "steered route unrepeatable (docs/issues/0110, 0114)",
    "g_TitlescreenState m_Tick/m_SubTick (informational only)":
        "the console's card I/O takes about 50 frames where the HLE card takes 6, so later ticks differ",
    "scratchpad 0x1F800000..0x1F800400": "the console reference exposes main RAM only",
    "moby arrays, particle pools, draw environments and packet pools":
        "allocation/rendering state, not gameplay state (docs/oracle.md retained observations)",
    "VRAM, SPU and CD device state": "separate pixel, audio and hardware questions",
    "the pad words and the per-field pad ring (informational only)":
        "the product parks after the VBlank polls of its step and the console before them, so the words hold the "
        "next frame's input on one core; the same input reaching the same update shows in the player's state",
}

# Artisans input after arrival: a held-nothing settle, then run, turn, jump, charge and running jump.
gameplay = (
    (frozenset(), 120),
    (frozenset({"up"}), 60),
    (frozenset(), 30),
    (frozenset({"left"}), 45),
    (frozenset(), 30),
    (frozenset({"cross"}), 6),
    (frozenset(), 60),
    (frozenset({"square"}), 6),
    (frozenset(), 60),
    (frozenset({"up", "cross"}), 6),
    (frozenset({"up"}), 30),
    (frozenset(), 30),
)



def lookahead(core: CoreSession) -> int:
    """The console parks right after the main loop's update, so the next two fields poll a hold before the update
    that reads it. The product's step runs the update and then its fields (FrameDriver::stepFrame), so its next
    update reads fields polled before the hold and the one after that is the first to see it."""
    return 0 if core.reference else 1


TAP_PERIOD = 20  # tools/drive.py Navigator: a 4-frame tap every 20-frame observation step
TAP_WIDTH = 4


@dataclass(frozen=True)
class Observation:
    gamestate: int
    title: TitleState
    game_tick: int
    level: int

    @property
    def at_save_picker(self) -> bool:
        return self.gamestate == GS_TITLE_SCREEN and self.title.mode == TSM_LOADING

    @property
    def playing(self) -> bool:
        return self.gamestate == GS_PLAYING


def observe(core: CoreSession) -> Observation:
    raw = core.read(guest_globals.kTitlescreenState, 24)
    words = [int.from_bytes(raw[i:i + 4], "little") for i in range(0, 24, 4)]
    return Observation(u32(core, guest_globals.kGamestate), TitleState(*words), u32(core, guest_globals.kGameTick),
                       u32(core, guest_globals.kLevelId))


def summary(core: CoreSession) -> dict:
    seen = observe(core)
    return {"gamestate": seen.gamestate, "title": (seen.title.mode, seen.title.state, seen.title.sub_state),
            "tick": seen.game_tick, "level": seen.level}


def advance(core: CoreSession, frames: int, strict: bool = False) -> None:
    """Step `frames` guest main-loop iterations.

    The console parks per field and ends a frame when the main loop consumes g_UnprocessedFrames. The product's
    step is a whole logic frame once its boot prefix is done (titles/spyro1/core/frame/frame_driver.cpp), and the
    first frame update is what writes g_DeltaTime, so until that word is set a step runs no guest frame."""
    for _ in range(frames):
        if core.reference:
            step_until_counter_resets(core, guest_globals.kUnprocessedFrames)
            continue
        core.step(1)
        while u32(core, guest_globals.kDeltaTime) == 0:
            core.step(1)


def _tapping(frame: int) -> bool:
    return frame % TAP_PERIOD < TAP_WIDTH


def _pattern(prompt: Callable[[title_prompts.Screen], title_prompts.Prompt]):
    def pattern(frame: int, seen: Observation) -> frozenset[str]:
        if not _tapping(frame):
            return frozenset()
        return frozenset(prompt(title_prompts.Screen(seen.gamestate, seen.title)).buttons)
    return pattern


title_menu_pattern = _pattern(title_prompts.title_menu_prompt)
new_game_pattern = _pattern(title_prompts.new_game_prompt)


def reach_save_picker(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    return driver.drive(core, budget, lambda seen: seen.at_save_picker, title_menu_pattern, "save picker", settle)


def reach_playing(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    return driver.drive(core, budget, lambda seen: seen.playing, new_game_pattern, "GS_Playing", settle)


# g_GameTick past the Artisans intro on both cores, so the pictures show one moment (docs/issues/0126).
SETTLED_GAME_TICK = 180


def reach_settled_play(driver: Driver, core: CoreSession, budget: int, settle: Settle) -> tuple[int, frozenset[str]]:
    return driver.drive(core, budget,
                        lambda seen: seen.playing and seen.game_tick >= SETTLED_GAME_TICK,
                        new_game_pattern, f"GS_Playing at g_GameTick {SETTLED_GAME_TICK}", settle)


checkpoints = (
    Checkpoint("save_picker", reach_save_picker),
    Checkpoint("playing", reach_playing,
               not_picture_comparable=(
                   "GS_Playing becomes true during the level intro, whose phase is in no declared "
                   "range (docs/issues/0126); `settled_play` is the picture-comparable state")),
    Checkpoint("settled_play", reach_settled_play),
    # No level-entry checkpoint: walking out is camera-relative and does not replay (issue 0114).
)
