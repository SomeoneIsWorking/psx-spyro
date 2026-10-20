#!/usr/bin/env python3
"""Replay a recorded `.pad` through the built product, headless, and report the level it lands in.

The run uses a private card seeded from the recording's card image (the product refuses a card whose
SHA-256 differs) and nothing but PSXPORT_PAD_REPLAY touches the pad.

    uv run --frozen python tools/pad_replay.py \\
        --pad replays/gameplay/artisans-arrival.pad \\
        --card-seed scratch/saves/pad_seed.mcr --steps 3300 --shot scratch/pad.fpm
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
from pathlib import Path

import drive
import guest_globals
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from title_states import GS_PLAYING

ROOT = drive.ROOT

# A pad leg's private card, never the shared scratch/saves/card.mcr (docs/issues/0169).
PAD_CARD = ROOT / "scratch" / "saves" / "pad_replay.mcr"


def card_digest(path: Path) -> str:
    """The SHA-256 of a whole card image, which is the identity a recording carries."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def seed_card(card: Path, seed: Path | None) -> str:
    """Copy `seed` (the card at recording time) over the private card and return its digest."""
    card.parent.mkdir(parents=True, exist_ok=True)
    if seed is not None:
        if not seed.is_file():
            raise SystemExit(f"[pad-replay] REFUSED: --card-seed {seed} does not exist. It is the card "
                             "image the recording was made against; the run must start from it")
        shutil.copyfile(seed, card)
    if not card.is_file():
        raise SystemExit(f"[pad-replay] REFUSED: no card at {card} and no --card-seed given. Point "
                         "--card-seed at the image the capture was recorded on")
    return card_digest(card)


def environment(settings: Path, card: Path, pad: Path, extra: dict[str, str]) -> dict[str, str]:
    """The agent environment plus this leg's card and the replay knob; `card=` would delete the card."""
    env = drive.environment("spyro1", settings)
    env["PSXPORT_CARD"] = str(card)
    env["PSXPORT_PAD_REPLAY"] = str(pad)
    env.update(extra)
    return env


def run(args: argparse.Namespace) -> int:
    pad = Path(args.pad).resolve()
    if not pad.is_file():
        print(f"[pad-replay] REFUSED: no pad file at {pad}")
        return 2
    card = Path(args.card).resolve() if args.card else PAD_CARD
    digest = seed_card(card, Path(args.card_seed).resolve() if args.card_seed else None)
    settings = Path(args.settings).resolve() if args.settings else drive.SHIPPING_SETTINGS
    log = Path(args.log).resolve()
    log.parent.mkdir(parents=True, exist_ok=True)
    extra = {}
    for spec in args.env:
        key, _, value = spec.partition("=")
        extra[key] = value
    try:
        executable, image = drive.launch_inputs("spyro1", ROOT / args.executable)
        env = environment(settings, card, pad, extra)
    except (OSError, ToolError) as refusal:
        print(f"[pad-replay] REFUSED: {refusal}")
        return 2

    print(f"[pad-replay] pad    {pad}")
    print(f"[pad-replay] card   {card} ({digest[:16]}…)")
    print(f"[pad-replay] setts  {settings.name}")
    print("[pad-replay] NO input driver: every press comes from the pad file")

    port = drive.Port(executable, image, log, env)
    previous = None
    left_boot_at = None
    arrived_at = None
    try:
        for step in range(args.steps):
            port._send("run 1")
            port._await_prompt()
            key = (port.gamestate(), port.word(guest_globals.kLevelId))
            if key != previous:
                print(f"  step {step}: gs={key[0]}/level={key[1]:#x}")
            # gs == GS_PLAYING is also the boot prefix, so arrival is a LEVEL, not a gamestate.
            if key[0] == GS_PLAYING and key[1] != 0 and left_boot_at is not None:
                if arrived_at is None:
                    arrived_at = step
            if key[0] != GS_PLAYING or key[1] != 0:
                left_boot_at = step
            previous = key
        if args.shot:
            Path(args.shot).parent.mkdir(parents=True, exist_ok=True)
            port.shot(str(Path(args.shot).resolve()))
            print(f"[pad-replay] shot  {args.shot}")
    finally:
        port.end()

    if arrived_at is not None:
        print(f"ARRIVED IN A LEVEL (gs={GS_PLAYING}, level!=0) at step {arrived_at}")
    else:
        print(f"NEVER ARRIVED IN A LEVEL in {args.steps} steps; last state "
              f"gs={previous[0]}/level={previous[1]:#x}")
        print("  (gs=0/level=0 is the boot prefix, not gameplay; a run that stays there never played)")
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--pad", required=True, help="the .pad recording to replay")
    parser.add_argument("--card-seed", default="",
                        help="the card image as it was at RECORDING time; copied into the private "
                             "card before the run so the start state is the recorded one")
    parser.add_argument("--card", default="", help="the private card image (default scratch/saves/pad_replay.mcr)")
    parser.add_argument("--steps", type=int, default=3300,
                        help="product steps to replay, one REPL `run 1` each (two fields in gameplay)")
    parser.add_argument("--shot", default="", help="capture one frame at the end, as a .ppm")
    parser.add_argument("--executable", default=str(drive.EXECUTABLE))
    parser.add_argument("--settings", default="")
    parser.add_argument("--log", default="scratch/logs/pad_replay.log")
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    return run(parser.parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
