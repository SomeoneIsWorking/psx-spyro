#!/usr/bin/env python3
"""Run a Spyro 2 or Spyro 3 boot headless and report what it did, with denominators.

    uv run --frozen python tools/boot_run.py --title spyro2
    uv run --frozen python tools/boot_run.py --title spyro3 --frames 600

Prints fields, product steps, presentation fences, translated and executed blocks, faults, fallbacks
by reason, CD reads against delivered completions, and the named stop or resume address; the log
parser is `boot_log.py`. Exit 2 refuses a missing port, image or disc; exit 1 is a log that names
neither a resume nor a stop, or (with `--require-cd-match`) reads that differ from deliveries.
The log goes to `scratch/boot/<title>.log`.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

import boot_log
import drive
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from title_profile import TITLES, TitleProfile

ROOT = drive.ROOT
EXIT_OK, EXIT_UNREADABLE_OR_MISMATCH, EXIT_REFUSED = 0, 1, 2

# `cd` logs each CdRead, `cdirq` each queued and delivered completion.
BOOT_CHANNELS = "cd,cdirq"


def boot_environment(title: TitleProfile, frames: int | None = None) -> dict[str, str]:
    env = drive.environment(title.slug)
    env.pop("PSXPORT_REPL", None)  # a boot run reads no commands; the REPL would wait for stdin
    env["PSXPORT_DEBUG"] = ",".join(filter(None, (env.get("PSXPORT_DEBUG"), BOOT_CHANNELS)))
    if frames is not None:
        env["PSXPORT_NATIVE_FRAMES"] = str(frames)
    return env


def run_boot(
    title: TitleProfile,
    executable: Path,
    image: Path,
    env: Mapping[str, str],
    log: Path,
    timeout: int,
) -> str:
    """Launch the port once and return its combined output, also written to `log`."""
    log.parent.mkdir(parents=True, exist_ok=True)
    try:
        completed = subprocess.run(
            [str(executable), str(image)],
            env=dict(env),
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
        output = completed.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired as expired:
        output = (expired.stdout or b"").decode("utf-8", "replace")
        log.write_text(output)
        raise ToolError(f"{title.label} did not end within {timeout}s; the partial log is {log}") from expired
    log.write_text(output)
    return output


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--title", choices=sorted(TITLES), required=True)
    parser.add_argument("--executable", type=Path, default=drive.EXECUTABLE)
    parser.add_argument(
        "--frames",
        type=int,
        default=None,
        help="stop after this many presentation-fence steps (PSXPORT_NATIVE_FRAMES): a run length, reported as a cap",
    )
    parser.add_argument("--timeout", type=int, default=400)
    parser.add_argument("--require-cd-match", action="store_true")
    args = parser.parse_args(argv)
    title = TITLES[args.title]
    log = ROOT / "scratch/boot" / f"{args.title}.log"
    try:
        executable, image = drive.launch_inputs(title.slug, args.executable)
        env = boot_environment(title, args.frames)
        output = run_boot(title, executable, image, env, log, args.timeout)
    except (OSError, ToolError) as refusal:
        print(f"boot_run: REFUSED: {refusal}", file=sys.stderr)
        return EXIT_REFUSED
    report = boot_log.parse(output)
    print(boot_log.render(report, title=title.label))
    print(f"  log: {log}")
    if not report.end_named:
        return EXIT_UNREADABLE_OR_MISMATCH
    if args.require_cd_match and not report.cd_matches:
        return EXIT_UNREADABLE_OR_MISMATCH
    return EXIT_OK



if __name__ == "__main__":
    sys.exit(main())
