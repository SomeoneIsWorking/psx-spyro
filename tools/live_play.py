#!/usr/bin/env python3
"""Play Spyro 1 at full speed over the live debug server, hold buttons in gameplay, and report what moved.

    uv run --frozen python tools/live_play.py
    uv run --frozen python tools/live_play.py --hold left --seconds 20 --port 5971

Unlike `drive.py`, the product keeps running between commands; `title_prompts.py` picks the menu buttons.
"""

from __future__ import annotations

import argparse
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

import boot_log
import drive
import guest_globals
import title_prompts
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from dbgclient import LiveClient  # noqa: E402
from title_states import TitleState

ROOT = drive.ROOT
OUT_DIR = ROOT / "scratch" / "live"
OBSERVE_FRAMES = 20  # frames between decisions, as drive.py's Navigator uses


def launch(executable: Path, binary: Path, port: int, log: Path,
           settings: Path | None = None) -> subprocess.Popen:
    """The product, headless and silent, serving the live endpoint on `port` with `settings` (default shipping)."""
    environment = drive.environment("spyro1", settings)
    # A product waiting for REPL input on stdin would never present.
    environment.pop("PSXPORT_REPL", None)
    environment["PSXPORT_DEBUG_SERVER"] = str(port)
    environment["PSXPORT_LOG_FILE"] = str(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text("")
    return subprocess.Popen([str(executable), str(binary)], cwd=ROOT, env=environment,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def connect(port: int, seconds: float) -> LiveClient:
    """Wait for the endpoint; refuse when it never answers."""
    deadline = time.monotonic() + seconds
    last: OSError | None = None
    while time.monotonic() < deadline:
        try:
            client = LiveClient(port)
            client.frame()
            return client
        except OSError as error:
            last = error
            time.sleep(0.5)
    raise SystemExit(f"REFUSED: the live endpoint never answered on 127.0.0.1:{port} within {seconds:.0f}s "
                     f"({last}); see the run's log")


class Session:
    """Reach gameplay through the menus, then hold the requested buttons and report what moved."""

    def __init__(self, client: LiveClient, log: Path) -> None:
        self.client = client
        self.log = log
        self.presses = 0
        self.first_frame = client.frame()

    def screen(self) -> title_prompts.Screen:
        return title_prompts.Screen(
            gamestate=self.client.word(guest_globals.kGamestate),
            title=TitleState(*self.client.words(guest_globals.kTitlescreenState, 6)),
            level_trans_hud=self.client.word(guest_globals.kLevelTransHudActive))

    def answer(self, prompt: title_prompts.Prompt) -> bool:
        if prompt.refuse:
            raise SystemExit(f"REFUSED: {prompt.refuse} at {self.screen()}")
        if not prompt.buttons:
            return False
        for button in prompt.buttons:
            self.client.tap(button)
            self.presses += 1
        return True

    def advance(self, frames: int = OBSERVE_FRAMES) -> int:
        """Wait for `frames` presented frames by watching the product's frame counter."""
        target = self.client.frame() + frames
        while self.client.frame() < target:
            time.sleep(0.01)
        return self.client.frame()

    def drive_to_gameplay(self, budget_frames: int) -> int:
        """Boot -> title -> save picker -> playable level, in the three legs title_prompts owns."""
        legs = (("save picker", title_prompts.title_menu_prompt),
                ("new game", title_prompts.new_game_prompt),
                ("GS_Playing", title_prompts.load_route_prompt))
        spent = 0
        leg = 0
        while spent < budget_frames and leg < len(legs):
            name, decide = legs[leg]
            screen = self.screen()
            if decide(screen).reached:
                print(f"[live] {name} reached at {screen.gamestate} "
                      f"title={screen.title.mode}/{screen.title.state} frame={self.advance(0)}")
                leg += 1
                continue
            self.answer(decide(screen))
            self.advance()
            spent += OBSERVE_FRAMES
        if leg < len(legs):
            raise SystemExit(f"REFUSED: reached only {leg} of {len(legs)} route legs "
                             f"({', '.join(name for name, _ in legs[:leg])}) within {budget_frames} frames; "
                             f"last {self.screen()}")
        return spent

    def hold(self, buttons: tuple[str, ...], seconds: float) -> dict:
        """Hold buttons for a wall-clock window while the game runs, and report what moved."""
        for button in buttons:
            self.client.press(button)
        before = self.client.frame()
        started = time.monotonic()
        position_before = self.client.words(guest_globals.kSpyro, 3)
        ticks_before = self.client.word(guest_globals.kGameTick)
        while time.monotonic() - started < seconds:
            self.advance(OBSERVE_FRAMES)
        for button in buttons:
            self.client.release(button)
        after = self.client.frame()
        return {
            "buttons": list(buttons),
            "seconds": round(time.monotonic() - started, 1),
            "frames": after - before,
            "game_tick": self.client.word(guest_globals.kGameTick) - ticks_before,
            "player_before": position_before,
            "player_after": self.client.words(guest_globals.kSpyro, 3),
        }


def effective_configuration(client: LiveClient) -> dict:
    """Each knob's current value and source layer, from the product's `cvars` reply."""
    reply = client.send("cvars")
    knobs: dict[str, str] = {}
    layers: dict[str, str] = {}
    for line in reply.splitlines():
        # "  NAME  kind = value [layer]"; a value may contain spaces.
        match = re.match(r"\s+(\S+)\s+(\S+)\s+=\s+(.*?)\s+\[([^\]]+)\]", line)
        if not match:
            continue
        knobs[match.group(1)] = match.group(3).strip()
        layers[match.group(1)] = match.group(4)
    audit = next((line for line in reply.splitlines() if line.startswith("env audit:")), "")
    return {"knobs": knobs, "layers": layers, "env_audit": audit.strip(),
            "unmatched": sorted({line.split("UNKNOWN ")[1].split(" ")[0]
                                 for line in reply.splitlines() if line.strip().startswith("UNKNOWN ")})}


def guest_execution(client: LiveClient) -> dict:
    """The executor's ledger now, keyed by its `guest` and `fallback` sections."""
    return boot_log.ledger_sections(client.send("guest"))


def report_run(client: LiveClient) -> None:
    config = effective_configuration(client)
    knobs = config["knobs"]
    interesting = ("PSXPORT_WIDE", "PSXPORT_FPS60", "PSXPORT_RENDER_PATH", "PSXPORT_FRAMES",
                   "PSXPORT_IRES", "PSXPORT_SETTINGS", "PSXPORT_DEBUG_SERVER")
    print("[live] configuration, asked of the product:")
    for name in interesting:
        if name in knobs:
            print(f"[live]   {name} = {knobs[name]} [{config['layers'][name]}]")
    if config["unmatched"]:
        print(f"[live]   env variables that matched no knob: {config['unmatched']}")

    counters = guest_execution(client)
    dynarec = counters.get("guest", {})
    fallback = counters.get("fallback", {})
    executed = dynarec.get("executed_blocks", 0)
    print(f"[live] guest execution: {executed} executed blocks, "
          f"{dynarec.get('executed_instructions', 0)} instructions, "
          f"{dynarec.get('translated_blocks', 0)} translated, "
          f"{dynarec.get('cache_hits', 0)} cache hits / {dynarec.get('cache_misses', 0)} misses, "
          f"{dynarec.get('invalidations', 0)} invalidations, {dynarec.get('faults', 0)} faults")
    print(f"[live] interpreter fallback: {fallback.get('calls', -1)} calls, "
          f"{fallback.get('instructions', -1)} instructions, "
          f"{fallback.get('refused_calls', -1)} refused")
    if executed <= 0:
        raise SystemExit("REFUSED: the product presented frames but executed no guest blocks; "
                         "a play-through with no dynarec execution is not gameplay evidence")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--executable", type=Path, default=drive.EXECUTABLE)
    parser.add_argument("--port", type=int, default=5971, help="live endpoint port (default 5971)")
    parser.add_argument("--hold", nargs="*", default=["left"], metavar="BUTTON",
                        help="buttons to hold once gameplay is reached (default: left)")
    parser.add_argument("--seconds", type=float, default=15.0, help="how long to hold them")
    parser.add_argument("--budget", type=int, default=12000, help="frames allowed to reach gameplay")
    parser.add_argument("--connect-timeout", type=float, default=180.0)
    parser.add_argument("--shot", type=Path, default=OUT_DIR / "gameplay.ppm")
    args = parser.parse_args()

    log = OUT_DIR / "live_play.log"
    try:
        executable, image = drive.launch_inputs("spyro1", ROOT / args.executable)
        product = launch(executable, image, args.port, log)
    except (OSError, ToolError) as refusal:
        print(f"REFUSED: {refusal}", file=sys.stderr)
        return 2
    try:
        client = connect(args.port, args.connect_timeout)
        session = Session(client, log)
        print(f"[live] endpoint on 127.0.0.1:{args.port}, product pid {product.pid}, "
              f"presenting from frame {session.first_frame}")
        route_frames = session.drive_to_gameplay(args.budget)
        print(f"[live] gameplay reached after {route_frames} frames and {session.presses} menu answers")

        args.shot.parent.mkdir(parents=True, exist_ok=True)
        shot_reply = session.client.shot(str(args.shot)).strip()
        shot_bytes = args.shot.stat().st_size if args.shot.is_file() else 0
        print(f"[live] shot: {shot_reply} ({shot_bytes} bytes)")

        held = session.hold(tuple(args.hold), args.seconds) if args.hold else {}
        if held:
            moved = held["player_before"] != held["player_after"]
            print(f"[live] held {held['buttons']} for {held['seconds']}s: {held['frames']} presented "
                  f"frames, {held['game_tick']} game ticks, player {held['player_before']} -> "
                  f"{held['player_after']} ({'MOVED' if moved else 'did not move'})")

        report_run(client)
        session.client.quit()
        return 0
    finally:
        if product.poll() is None:
            product.send_signal(signal.SIGTERM)
            try:
                product.wait(timeout=30)
            except subprocess.TimeoutExpired:
                product.kill()
                product.wait(timeout=30)
        print(f"[live] product pid {product.pid} exited {product.returncode}")


if __name__ == "__main__":
    raise SystemExit(main())
