#!/usr/bin/env python3
"""pad_replay.py — replay a recorded `.pad` through the built product, headless, and say where it lands.

WHY A TOOL AND NOT A SCRIPT LINE. A `.pad` file is a deterministic input capture, so the run that
replays one has exactly one thing going right: the presses come out of the file and nothing else
touches the pad. That property is easy to lose. A driver in the loop, a card that differs from the
one the capture was made on, or a card shared with the next run each produce a run that *looks*
like a replay and answers a different screen. The first is what left this repository's only gameplay
pad describing a memory-card prompt for its whole run (docs/issues/0116), and the file could not say
so.

WHAT THIS OWNS, and what it deliberately does not:

  - THE CARD is private and is in the state the recording was made against. Every recording carries
    the SHA-256 of its card image and the product refuses a different one by name, so a run on the
    wrong card is a refusal, not a wrong answer. This tool supplies that card; it never re-checks
    the digest itself, because the product already refuses and a second answer to the same question
    is a second thing to be wrong.

  - WHY `card=` IS NOT USED HERE, stated because it looks like an omission. drive.environment(card=)
    runs blank_card_environment, which DELETES the card so the product formats a fresh one. A pad
    leg is the one run whose start state is fixed by something outside itself: the recording's card
    identity. Deleting it here would guarantee a refusal. So the card is a private image restored
    from `--card-seed` — the isolation is the same private path, and the start state is the recorded
    one. Spyro 2/3's title routes, which must merely repeat and not match a capture, keep using
    drive.ROUTE_CARD and the blank-card path unchanged.

  - THE PRODUCT is launched with its executable path and the disc argument, exactly as every other
    maintainer tool here does. Zero-argument `spyro_port` is the player's title selector; a pad
    recording belongs to the title it was captured on, so the capture names it.

  - NOTHING DRIVES THE PAD. There is no tap, no press and no driver in this file. The only way a
    button reaches the guest is through PSXPORT_PAD_REPLAY, and that is the claim the run makes.

The report names the state it actually observed, and "arrived" means a level, not a gamestate: Spyro
runs its boot prefix at gamestate 0, so a probe that only asks for gamestate 0 reports arrival at
step 0 and describes a logo.

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

# A pad leg's private card, seeded from the image the capture was made against. Never
# scratch/saves/card.mcr: that is the shared card, and a run that writes it changes the next run's
# input (docs/issues/0169).
PAD_CARD = ROOT / "scratch" / "saves" / "pad_replay.mcr"


def card_digest(path: Path) -> str:
    """The SHA-256 of a whole card image, which is the identity a recording carries."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def seed_card(card: Path, seed: Path | None) -> str:
    """Put the card in the state the capture expects and return the digest it now has.

    `--card-seed` is the copy of the card as it was at RECORDING time. Both legs start from it, so
    the two runs differ only in what the pad file asks for. With no seed the existing private card
    is used as it stands, and its digest is reported either way, because a run must be able to say
    which card it was on.
    """
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
    """The agent environment plus this leg's private card and the replay knob.

    The card is set here rather than through `card=`, which deletes it: a pad leg starts on the recorded card."""
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
