#!/usr/bin/env python3
"""Drive Spyro 1 to gameplay over the REPL, choosing each pad edge from the guest's own state words.

The words come from `guest_globals.py`; nothing is written into guest memory.

Usage:
  drive.py gameplay --shot scratch/screenshots/gameplay.ppm
  drive.py gameplay --hold left --hold-steps 30 --shot scratch/screenshots/left.ppm
  drive.py gameplay --tap circle --after 120 --shot scratch/screenshots/flame.ppm
  drive.py gameplay --press-while 0:start --shot scratch/screenshots/boot-pressed.ppm
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import guest_globals
import press_conditions
import provision_title
import route_scenes
import title_prompts
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from port.launch_environment import agent_environment  # noqa: E402
from repl_walk import Seeker
from spyro1_steering import moby_class_targets, portal_targets
from title_identity import ROOT
from title_states import GAMESTATE_NAMES, GAMESTATE_PRESENTATION, GS_EXIT_LEVEL, GS_FLIGHT_RESULTS, TitleState

# The agent build every driver launches unless told otherwise.
EXECUTABLE = ROOT / "build" / "agent" / "bin" / "spyro_port"
# Tracked, so a run never depends on an untracked psxport_settings.ini in the working directory.
SHIPPING_SETTINGS = ROOT / "tools" / "shipping_settings.ini"
# Deleted before each route that must repeat, so a previous run's save is not the next boot state.
ROUTE_CARD = ROOT / "scratch" / "saves" / "route.mcr"
# The fairy scene writes a save, so scenes get their own card.
SCENE_CARD = ROOT / "scratch" / "saves" / "scene.mcr"

# Pause-menu selected entry and submenu (update 0x8002E12C); 0xA8 apart, so two reads.
G_PAUSE_OPTION = 0x80075720
G_PAUSE_SUBMENU = 0x800757C8


class Port:
    """One live port process driven through its REPL control channel."""

    _WORDS = re.compile(r"\[repl\] ([0-9A-F]{8}):((?: [0-9A-F]{8})+)")
    _READY = re.compile(r"\[repl\] frame=(\d+) ready")

    def __init__(self, executable: Path, binary: Path, log: Path, env: dict[str, str],
                 post_presses: "press_conditions.PressConditions | None" = None,
                 gamestate_address: int = guest_globals.kGamestate):
        # Spyro 2/3 pass their own word from title_profile.py.
        self._gamestate_address = gamestate_address
        log.parent.mkdir(parents=True, exist_ok=True)
        self._log = log.open("w")
        self._proc = subprocess.Popen(
            [str(executable), str(binary)],
            cwd=ROOT,
            env=env,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        self.frame = 0
        # Applied by run(), so screens reached after arrival (tally, sweep, glide) can be pressed.
        self._post_presses = (post_presses if post_presses is not None
                              else press_conditions.PressConditions(phase="post-arrival"))
        # Samples per gamestate over the whole run.
        self.gamestate_census: dict[int, int] = {}
        # -1 until the first sample.
        self._last_sampled_gamestate = -1
        # The census as it stood at GS_Playing, so later samples can be told from the approach.
        self._census_at_arrival: dict[int, int] = {}
        self._await_prompt()

    # -- public API ---------------------------------------------------------

    # Product steps per sample, short enough to see a state the game passes through quickly.
    SAMPLE_STEPS = 5

    def run(self, steps: int) -> int:
        remaining = max(1, steps)
        result = 0
        while remaining > 0:
            step = min(self.SAMPLE_STEPS, remaining)
            self._send(f"run {step}")
            result = self._await_prompt()
            remaining -= step
            state = self.gamestate()
            self._last_sampled_gamestate = state
            self.gamestate_census[state] = self.gamestate_census.get(state, 0) + 1
            for condition in self._post_presses.apply(self, state):
                print(f"press: {condition.name()} at frame {self.frame} (post-arrival)",
                      file=sys.stderr)
        return result

    def mark_arrival(self) -> None:
        self._census_at_arrival = dict(self.gamestate_census)

    @property
    def last_sampled_gamestate(self) -> int:
        """The gamestate `run` last sampled, or -1 before the first sample."""
        return self._last_sampled_gamestate

    def census_line(self) -> str:
        """One line naming every gamestate sampled, the ones never reached, and those since arrival."""
        total = sum(self.gamestate_census.values())
        if not total:
            return "gamestates: nothing sampled (no steps were advanced)"
        seen = ", ".join(f"{GAMESTATE_NAMES.get(state, str(state))}={count}"
                         for state, count in sorted(self.gamestate_census.items()))
        missing = [f"{name} ({GAMESTATE_PRESENTATION.get(state, '?')})"
                   for state, name in sorted(GAMESTATE_NAMES.items())
                   if state not in self.gamestate_census]
        line = (f"gamestates over {total} samples ({self.SAMPLE_STEPS} steps apart): {seen}"
                + (f"; never reached: {', '.join(missing)}" if missing else ""))
        after = {state: count - self._census_at_arrival.get(state, 0)
                 for state, count in self.gamestate_census.items()
                 if count - self._census_at_arrival.get(state, 0) > 0}
        if not self._census_at_arrival:
            return line + "; gameplay never started, so none of this is a gameplay observation"
        since = ", ".join(f"{GAMESTATE_NAMES.get(state, str(state))}={count}"
                          for state, count in sorted(after.items()))
        return line + f" | since GS_Playing: {since or 'nothing sampled'}"

    def tap(self, button: str, frames: int = 4) -> None:
        self._send(f"tap {button} {frames}")

    def press(self, button: str) -> None:
        self._send(f"press {button}")

    def release(self, button: str) -> None:
        self._send(f"release {button}")

    def words(self, address: int, count: int = 1) -> list[int]:
        self._send(f"rw {address:X} {count}")
        return self._await_words(address, count)

    def word(self, address: int) -> int:
        return self.words(address, 1)[0]

    def gamestate(self) -> int:
        return self.word(self._gamestate_address)

    def pause_menu(self) -> tuple[int, int]:
        """The pause menu's selected entry (`D_80075720`) and its submenu flag (`D_800757C8`)."""
        return self.word(G_PAUSE_OPTION), self.word(G_PAUSE_SUBMENU)

    def title(self) -> TitleState:
        w = self.words(guest_globals.kTitlescreenState, 6)
        return TitleState(w[0], w[1], w[2], w[3], w[4], w[5])

    def gates(self) -> None:
        """Ask the port to list this level's gates, their target levels and path nodes."""
        self._send("gates")

    def gate_teleport(self, gate: int, node: int) -> None:
        """Place Spyro on a gate's path node through the port's gate diagnostic; the walk cannot climb."""
        self._send(f"gate-teleport {gate} {node}")

    def shot(self, path: str) -> None:
        """Capture the presented picture to `path`, returning once the port has written it (its own
        `[repl] shot ... -> <path>` line), so a caller can read the file at once."""
        Path(ROOT / path).parent.mkdir(parents=True, exist_ok=True)
        self._send(f"shot {path}")
        for line in self._lines():
            if "[repl] shot" in line and f"-> {path}" in line:
                return
        raise ToolError(f"the port exited before writing the shot {path}")

    def dumpram(self, path: str) -> None:
        Path(ROOT / path).parent.mkdir(parents=True, exist_ok=True)
        self._send(f"dumpram {path}")

    def repl_command(self, line: str) -> None:
        """Send one line to the product's REPL and let its owner answer."""
        self._send(line)

    def preseq(self, count: int, directory: str) -> None:
        """Dump the next `count` presented frames through the REPL's `preseq`."""
        target = ROOT / directory
        target.mkdir(parents=True, exist_ok=True)
        self._send(f"preseq {count} {target}")

    def end(self) -> int:
        # A port that already exited has no reader for `end`.
        if self._proc.poll() is None:
            self._send("end")
            assert self._proc.stdin is not None
            self._proc.stdin.close()
        self._drain()
        return self._proc.wait()

    # -- plumbing -----------------------------------------------------------

    def _send(self, line: str) -> None:
        assert self._proc.stdin is not None
        self._proc.stdin.write(line + "\n")
        self._proc.stdin.flush()

    def _lines(self):
        assert self._proc.stdout is not None
        for line in self._proc.stdout:
            self._log.write(line)
            yield line

    def _await_prompt(self) -> int:
        for line in self._lines():
            match = self._READY.search(line)
            if match:
                self.frame = int(match.group(1))
                return self.frame
        raise ToolError("the port exited before reaching its next frame boundary")

    def _await_words(self, address: int, count: int) -> list[int]:
        for line in self._lines():
            match = self._WORDS.search(line)
            if match and int(match.group(1), 16) == address:
                values = [int(v, 16) for v in match.group(2).split()]
                if len(values) == count:
                    return values
        raise ToolError(f"the port exited before answering rw 0x{address:08X}")

    def _drain(self) -> None:
        for _ in self._lines():
            pass
        self._log.close()



class Navigator:
    """Boot -> title -> save picker -> a loaded, playable level, decided from guest state."""

    STEP = 10  # product steps between observations; small enough to catch a one-shot menu state

    def __init__(self, port: Port, budget: int = 12000, skip_transitions: bool = False,
                 presses: "press_conditions.PressConditions | None" = None,
                 skip_button: str = "start"):
        self._port = port
        self._budget = budget
        self._skip_transitions = skip_transitions
        self._skip_button = skip_button
        # Only _advance() applies these, so a pre-arrival press cannot fire in gameplay.
        self._presses = presses if presses is not None else press_conditions.PressConditions()
        # (screen, button) presses already delivered; a screen can still read as itself after one.
        self._cancelled: set[tuple[str, str]] = set()

    def reach_gameplay(self) -> None:
        self._reach_title_menu()
        self._start_new_game()
        self._wait_for_playing()

    def _advance(self) -> None:
        """Run one observation step, then apply any pre-arrival press its condition now matches."""
        self._port.run(self.STEP)
        for condition in self._presses.apply(self._port, self._port.last_sampled_gamestate):
            print(f"press: {condition.name()} at frame {self._port.frame}", file=sys.stderr)

    def _screen(self) -> "title_prompts.Screen":
        return title_prompts.Screen(gamestate=self._port.gamestate(), title=self._port.title(),
                                    level_trans_hud=self._port.word(guest_globals.kLevelTransHudActive))

    def _answer(self, prompt: "title_prompts.Prompt") -> None:
        if prompt.refuse:
            raise ToolError(prompt.refuse)
        buttons = prompt.buttons
        if buttons and prompt.target and not prompt.repeatable:
            buttons = tuple(b for b in buttons if (prompt.target, b) not in self._cancelled)
            if buttons:
                self._cancelled.update((prompt.target, b) for b in buttons)
                print(f"cancel {prompt.target}: {'+'.join(buttons)}"
                      f"{f' held {prompt.hold_frames} fields' if prompt.hold_frames else ''}"
                      f" at frame {self._port.frame}", file=sys.stderr)
        for button in buttons:
            self._port.tap(button, prompt.hold_frames or 4)

    # title_prompts.py decides each screen's button; this driver owns the stepping and the budget.
    def _reach_title_menu(self) -> None:
        spent = 0
        while spent < self._budget:
            prompt = title_prompts.title_menu_prompt(self._screen())
            if prompt.reached:
                return
            self._answer(prompt)
            spent += self.STEP
            self._advance()
        raise ToolError(
            f"never reached the save picker (TSM_Loading) within {self._budget} steps; "
            f"last gamestate={self._port.gamestate()} title={self._port.title()}"
        )

    def _start_new_game(self) -> None:
        spent = 0
        while spent < self._budget:
            prompt = title_prompts.new_game_prompt(self._screen())
            if prompt.reached:
                return
            self._answer(prompt)
            spent += self.STEP
            self._advance()
        raise ToolError(
            f"the save picker never committed a slot within {self._budget} steps; "
            f"last title={self._port.title()}"
        )

    def _wait_for_playing(self) -> None:
        spent = 0
        while spent < self._budget:
            prompt = title_prompts.load_route_prompt(self._screen(), self._skip_transitions,
                                                      self._skip_button)
            if prompt.reached:
                return
            self._answer(prompt)
            spent += self.STEP
            self._advance()
        raise ToolError(
            f"never reached GS_Playing within {self._budget} steps; "
            f"gamestate={self._port.gamestate()} load_stage={self._port.word(guest_globals.kLoadStage)}"
        )


def quit_to_home(port: Port, budget: int = 4000) -> None:
    """Pause and take the guest's own Quit entry, the only way into GS_ExitLevel; refused in a homeworld."""
    level = port.word(guest_globals.kLevelId)
    if level % 10 == 0:
        raise ToolError(
            f"--quit-home needs a sub-level: level {level} is a homeworld, where the pause menu's "
            f"Quit opens a confirm screen instead of calling the exit-level entry (func_8002C618). "
            f"Cross a portal first (--seek-portal) or teleport onto a gate (--gate-teleport)"
        )
    port.tap("start")
    spent = 0
    while spent < budget:
        state = port.gamestate()
        if state == GS_EXIT_LEVEL:
            print(f"reached GS_ExitLevel at frame {port.frame}", file=sys.stderr)
            return
        if state == GS_FLIGHT_RESULTS:
            # func_8002E12C sends a flight level's Quit to the results screen instead.
            raise ToolError(
                f"--quit-home reached GS_FlightResults in level {level}: a flight level's Quit is the "
                f"results screen, not the return-home glide (func_8002E12C's g_IsFlightLevel arm)"
            )
        option, submenu = port.pause_menu()
        prompt = title_prompts.pause_quit_prompt(state, option, submenu)
        if prompt.refuse:
            raise ToolError(prompt.refuse)
        for button in prompt.buttons:
            print(f"quit-home: gamestate {state} option {option} submenu {submenu} -> {button}",
                  file=sys.stderr)
            port.tap(button)
        port.run(Navigator.STEP)
        spent += Navigator.STEP
    raise ToolError(f"the pause menu never handed off to GS_ExitLevel within {budget} steps; "
                  f"last gamestate={port.gamestate()}")


def environment(title: str = "spyro1", settings: Path | None = None,
                card: Path | None = None) -> dict[str, str]:
    """The hidden-window REPL environment for one title, with its disc resolved.

    `card` is deleted first so the run starts on a blank card; pass `ROUTE_CARD` for a route that must repeat."""
    spec = provision_title.SPECS[title]
    env = agent_environment(dict(os.environ), settings or SHIPPING_SETTINGS, card=card)
    env.update(PSXPORT_REPL="1", PSXPORT_WATCHDOG="0")
    env[spec.disc_env] = str(provision_title.resolve_disc(spec, None).path)
    return env


def launch_inputs(title: str, executable: Path) -> tuple[Path, Path]:
    """The built port and the title's provisioned image, refused by name when either is missing."""
    if not executable.is_file():
        raise ToolError(f"the built port is missing: {executable} - build target spyro_port first")
    image = ROOT / provision_title.SPECS[title].executable
    if not image.is_file():
        raise ToolError(f"{title} is not provisioned: {image} is missing - run tools/provision_title.py")
    return executable, image


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("target", choices=["gameplay"], help="the state to drive to")
    parser.add_argument("--executable", default=str(EXECUTABLE))
    parser.add_argument("--log", default="scratch/logs/drive.log")
    parser.add_argument("--debug", default="", help="PSXPORT_DEBUG channels for this run")
    parser.add_argument(
        "--env",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="extra PSXPORT_* knob for this run, e.g. --env PSXPORT_RENDER_PATH=device",
    )
    parser.add_argument(
        "--settings",
        default=str(SHIPPING_SETTINGS),
        help="PSXPORT_SETTINGS for this run; a missing file is refused",
    )
    parser.add_argument(
        "--settle",
        type=int,
        default=60,
        help="product steps to run after arrival before any input; control arrives after the fade-in",
    )
    parser.add_argument(
        "--skip-transitions",
        action="store_true",
        help="press Start or Cross on every skippable presentation on the route: the intro cutscene "
        "(held), the level-transition tally and the level flyby",
    )
    parser.add_argument(
        "--skip-button",
        choices=["start", "cross"],
        default="start",
        help="which button --skip-transitions presses; retail accepts either",
    )
    parser.add_argument(
        "--press-while",
        action="append",
        default=[],
        metavar="GAMESTATE:BUTTON[:FRAMES]",
        help="tap a button once, the first time the guest is seen in that gamestate before arrival. "
        "The states are "
        + ", ".join(f"{n}={s}" for s, n in sorted(GAMESTATE_NAMES.items()))
        + ".",
    )
    parser.add_argument(
        "--press-after",
        action="append",
        default=[],
        metavar="GAMESTATE:BUTTON[:FRAMES]",
        help="tap a button once, the first time the guest is seen in that gamestate at any time; "
        "GS_Playing, the pause menu and the inventory are refused. The states are "
        + ", ".join(f"{n}={s}" for s, n in sorted(GAMESTATE_NAMES.items()))
        + ".",
    )
    parser.add_argument("--hold", action="append", default=[], help="button held after arrival")
    parser.add_argument("--hold-steps", type=int, default=30, help="product steps the --hold buttons are held")
    parser.add_argument("--tap", action="append", default=[], help="button tapped after arrival")
    parser.add_argument("--after", type=int, default=0, help="product steps to run after the taps")
    parser.add_argument(
        "--repeat", type=int, default=1, help="how many times to repeat the tap/after cycle"
    )
    parser.add_argument(
        "--seek-class",
        type=int,
        default=-1,
        help="walk to the nearest live Moby of this class before the holds/taps, steering from the "
        "guest camera each step; Spyro 1's gems are classes 83..87",
    )
    parser.add_argument(
        "--seek-arrived",
        type=int,
        default=None,
        metavar="UNITS",
        help="stop the seek at this view-space distance instead of walking all the way in",
    )
    parser.add_argument(
        "--gate-teleport",
        default="",
        metavar="GATE:NODE",
        help="place Spyro on that gate's path node before anything else, using the port's own "
        "gate diagnostic; the walk cannot climb to a portal",
    )
    parser.add_argument(
        "--seek-portal",
        action="store_true",
        help="walk to the nearest level portal instead, which is how a homeworld route reaches a "
        "level; mutually exclusive with --seek-class",
    )
    parser.add_argument(
        "--scene",
        default="",
        choices=sorted(route_scenes.SCENES),
        help="drive one named scene (tools/route_scenes.py); exclusive with --seek-class, "
        "--seek-portal, --hold and --tap",
    )
    parser.add_argument(
        "--scene-proof",
        default="",
        metavar="PATH",
        help="write the scene's target, guest words and steps here as JSON; nothing when it failed",
    )
    parser.add_argument(
        "--quit-home",
        action="store_true",
        help="after the route, take the pause menu's Quit into GS_ExitLevel; refused in a homeworld",
    )
    parser.add_argument(
        "--quit-budget",
        type=int,
        default=4000,
        help="product steps --quit-home may spend answering the pause menu before refusing",
    )
    parser.add_argument(
        "--repl",
        action="append",
        default=[],
        metavar="COMMAND",
        help="send one raw line to the product's REPL at the end of the route",
    )
    parser.add_argument(
        "--card",
        default=None,
        help="memory-card image this run opens, deleted first; a --scene run defaults to "
        + str(SCENE_CARD.relative_to(ROOT)),
    )
    parser.add_argument("--shot", default="", help="capture here once the route and inputs are done")
    parser.add_argument(
        "--dumpram",
        default="",
        help="write guest main RAM here once the route and inputs are done",
    )
    parser.add_argument(
        "--preseq",
        type=int,
        default=0,
        help="capture the next N presented frames into --preseq-dir, in addition to --shot",
    )
    parser.add_argument(
        "--preseq-dir",
        default="scratch/screenshots/preseq",
        help="directory --preseq writes p%%04d.ppm into; it is not cleared first",
    )
    args = parser.parse_args()

    try:
        executable, image = launch_inputs("spyro1", ROOT / args.executable)
        env = environment("spyro1", ROOT / args.settings,
                          SCENE_CARD if args.scene and not args.card else args.card)
        presses = press_conditions.parse_all(args.press_while)
        post_presses = press_conditions.parse_post_all(args.press_after)
    except (OSError, ToolError) as refusal:
        parser.error(str(refusal))
    if args.debug:
        env["PSXPORT_DEBUG"] = args.debug
    for entry in args.env:
        name, separator, value = entry.partition("=")
        if not separator:
            parser.error(f"--env expects NAME=VALUE, got {entry!r}")
        env[name] = value

    port = Port(executable, image, ROOT / args.log, env, post_presses)
    try:
        Navigator(port, skip_transitions=args.skip_transitions, presses=presses,
                  skip_button=args.skip_button).reach_gameplay()
        print(f"reached GS_Playing at frame {port.frame}", file=sys.stderr)
        print(presses.report(), file=sys.stderr)
        print(post_presses.report(), file=sys.stderr)
        port.mark_arrival()
        if args.settle:
            port.run(args.settle)
        if args.gate_teleport:
            gate, separator, node = args.gate_teleport.partition(":")
            if not separator:
                parser.error(f"--gate-teleport expects GATE:NODE, got {args.gate_teleport!r}")
            port.gates()
            port.gate_teleport(int(gate), int(node))
            port.run(args.settle or 1)
        if args.seek_class >= 0 and args.seek_portal:
            parser.error("--seek-class and --seek-portal name two different destinations")
        if args.scene and (args.seek_class >= 0 or args.seek_portal or args.hold or args.tap):
            parser.error(
                "--scene decides its own inputs from guest state, so it cannot be combined with "
                "--seek-class, --seek-portal, --hold or --tap"
            )
        if args.scene:
            proof = route_scenes.run_scene(args.scene, port)
            print(f"scene {proof.scene}: proved {proof.target} in {proof.frames} step(s)",
                  file=sys.stderr)
            if proof.detail:
                print(f"scene {proof.scene}: {proof.detail}", file=sys.stderr)
            if args.scene_proof:
                target = ROOT / args.scene_proof
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(json.dumps(proof.as_json(), indent=2) + "\n")
            port.run(1)
            if not args.shot and not args.dumpram and not args.preseq:
                code = port.end()
                print(port.census_line(), file=sys.stderr)
                print(f"run log: {args.log} (exit {code})", file=sys.stderr)
                return code
        if args.seek_class >= 0:
            Seeker(
                port,
                f"class {args.seek_class}",
                moby_class_targets(port.words, args.seek_class),
                arrived=args.seek_arrived,
            ).walk()
        elif args.seek_portal:
            entering = port.word(guest_globals.kLevelId)
            Seeker(port, "portal", portal_targets(port.words), arrived=0,
                   stop=lambda: port.word(guest_globals.kLevelId) != entering,
                   stop_is=f"left level {entering}").walk()
        if args.quit_home:
            quit_to_home(port, budget=args.quit_budget)
        for button in args.hold:
            port.press(button)
        if args.hold:
            port.run(args.hold_steps)
            for button in args.hold:
                port.release(button)
        for _ in range(max(1, args.repeat)):
            for button in args.tap:
                port.tap(button, 8)
            if args.after:
                port.run(args.after)
        if args.preseq:
            port.preseq(args.preseq, args.preseq_dir)
            port.run(args.preseq + 1)
        if args.shot:
            port.shot(args.shot)
        if args.dumpram:
            port.dumpram(args.dumpram)
        if args.shot or args.dumpram:
            port.run(1)
        for command in args.repl:
            port.repl_command(command)
            port.run(1)
    except ToolError as refusal:
        print(f"drive.py REFUSED: {refusal}", file=sys.stderr)
        print(f"  {port.census_line()}", file=sys.stderr)
        print(f"  {presses.report()}", file=sys.stderr)
        print(f"  {post_presses.report()}", file=sys.stderr)
        print(f"  run log: {args.log}", file=sys.stderr)
        port.end()
        return 2
    code = port.end()
    print(port.census_line(), file=sys.stderr)
    print(post_presses.report(), file=sys.stderr)
    print(f"run log: {args.log} (exit {code})", file=sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
