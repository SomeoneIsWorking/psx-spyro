"""Shots of Spyro 1's attract demo at fixed fields, no input: the same frames before and after a render change.

The demo plays the same frames every run (demo_gputime.py's recipe), so two builds are compared picture for picture.
Runs hidden, silent and unpaced on a 3840x2160 sink; `ires` 0 is the judgement setting (8x there), 1 native.
`--recordcheck` also logs, per present, the display pixels that differ from the device (presentation.md, Check).

    uv run --frozen python tools/demo_shots.py scratch/screenshots/demo-8x --ires 0 [--at 0,300,600,900]
    uv run --frozen python tools/demo_shots.py scratch/screenshots/demo-1x --ires 1 --recordcheck [--present]

Writes `<dir>/f<field>.ppm` (the record picture) per field past the demo's load and `<dir>/run.log`; with
`--present` also `<dir>/p<field>.ppm`, what the window shows at the sink's size (`pshot`), the check for a
presentation change; prints recordcheck's mismatch counts when asked.
"""
import argparse
import re
import statistics
import sys
from pathlib import Path

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT))

import drive  # noqa: E402

TSM_DEMO = 3


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out", help="directory for the shots and run.log, under scratch/")
    parser.add_argument("--ires", type=int, default=0, help="internal resolution: 0 auto (8x on this sink), 1 native")
    parser.add_argument("--at", default="0,300,600,900", help="fields past the demo's load to shoot, ascending")
    parser.add_argument("--recordcheck", action="store_true", help="log display pixels that differ from the device")
    parser.add_argument("--present", action="store_true", help="also shoot what the window shows (pshot)")
    args = parser.parse_args()
    out = Path(args.out)
    (ROOT / out).mkdir(parents=True, exist_ok=True)
    log = (ROOT / out / "run.log").resolve()
    env = drive.environment("spyro1")
    env.update(
        PSXPORT_NOPACE="1",
        PSXPORT_SHOW_WINDOW="0",
        PSXPORT_NOAUDIO="1",
        PSXPORT_PRESENT_SINK="3840x2160",
        PSXPORT_IRES=str(args.ires),
    )
    if args.recordcheck:
        env["PSXPORT_DEBUG"] = "recordcheck"
    executable, image = drive.launch_inputs("spyro1", ROOT / "build" / "agent" / "bin" / "spyro_port")
    port = drive.Port(executable, image, log, env)
    port.repl_command(f"setires {args.ires}")
    for _ in range(1200):  # up to 6000 fields of idle title
        port.run(5)
        if port.title().mode == TSM_DEMO:
            break
    else:
        port.end()
        sys.exit("the attract demo never started")
    port.run(300)  # past the demo's load and fade-in
    done = 0
    for field in sorted(int(f) for f in args.at.split(",")):
        # run(0) still steps a field, which would put every later shot one field late.
        if field > done:
            port.run(field - done)
        done = field
        port.shot(str(out / f"f{field}.ppm"))
        if args.present:
            port.present_shot(str(out / f"p{field}.ppm"))
    port.end()
    print(f"shots in {out}: " + ", ".join(f"f{f}.ppm" for f in args.at.split(",")))
    if args.recordcheck:
        counts = [int(m.group(1)) for line in log.read_text().splitlines()
                  if (m := re.search(r"\[recordcheck\].* mismatched=(\d+) ", line))]
        if counts:
            print(f"recordcheck: {len(counts)} presents, differing pixels p50 {statistics.median(counts):.0f} "
                  f"max {max(counts)}")
        else:
            print("recordcheck: no mismatch counts parsed from run.log; read its recordcheck lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
