"""GPU time per present on Spyro 1's attract demo, no input: the scene DuckStation can be compared on.

DuckStation cannot be warped (no PINE in its macOS build), but left on the title screen it plays the same
attract demo as the port, so both are measured on the same gameplay frames. The run is the judgement setup
(ires 0 = 8x on a 3840x2160 sink, unpaced, hidden, silent) with `perf,gputime,perfframe` on; it waits for
TSM_Demo, lets the demo load for 300 fields, then meters `after` fields.

    uv run --frozen python tools/demo_gputime.py scratch/logs/demo-gputime.log [after=1800] [--cpu]

Prints the per-present GPU ms (p50/p90/max) over the demo and the run-end `[perf] gpu time` lines.
`--cpu` leaves gputime off, since its waits on every submit serialize CPU and GPU and inflate the present phase,
and prints the run-end `[perf] cpu` lines instead, for perf_compare.py.
Measured 2026-11-17 on an M2 (psxport ae780a3): p50 45.05, p90 52.03, max 64.59 ms/present; DuckStation
0.1-12640 (Metal, 8x, nearest) read 1.5-6 ms per frame on the same demo from its GPU overlay.
"""
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
    args = [a for a in sys.argv[1:] if a != "--cpu"]
    cpu = "--cpu" in sys.argv[1:]
    log = Path(args[0]).resolve()
    after = int(args[1]) if len(args) > 1 else 1800
    env = drive.environment("spyro1")
    env.update(
        PSXPORT_DEBUG="perf" if cpu else "perf,gputime,perfframe",
        PSXPORT_NOPACE="1",
        PSXPORT_SHOW_WINDOW="0",
        PSXPORT_NOAUDIO="1",
        PSXPORT_PRESENT_SINK="3840x2160",
        PSXPORT_IRES="0",
    )
    executable, image = drive.launch_inputs("spyro1", ROOT / "build" / "agent" / "bin" / "spyro_port")
    port = drive.Port(executable, image, log, env)
    port.repl_command("setires 0")
    start = None
    for _ in range(1200):  # up to 6000 fields of idle title
        port.run(5)
        if port.title().mode == TSM_DEMO:
            port.run(300)  # past the demo's load and fade-in
            start = port.frame
            break
    if start is None:
        port.end()
        sys.exit("the attract demo never started")
    port._log.flush()
    marker = sum(1 for _ in log.open())
    port.run(after)
    port.end()
    lines = log.read_text().splitlines()
    totals = [float(m.group(1)) for line in lines[marker:]
              if (m := re.search(r"present gpu-ms: total=([0-9.]+)", line))]
    print(f"demo started at frame {start}; {len(totals)} presents measured over {after} fields")
    if totals:
        q = statistics.quantiles(totals, n=10)
        print(f"gpu ms/present: p50 {statistics.median(totals):.2f}  p90 {q[8]:.2f}  max {max(totals):.2f}")
    for line in lines:
        if ("[perf] cpu" if cpu else "[perf] gpu time") in line:
            print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
