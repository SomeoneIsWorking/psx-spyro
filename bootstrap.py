"""Locked entry point for Spyro's player launcher: fetch external/psxport when missing, then run tools/run.py."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FRAMEWORK_FETCH = ("uv", "run", "--no-project",
                   "https://raw.githubusercontent.com/SomeoneIsWorking/psxport/main/tools/psxport_fetch.py", "--auto")

FRAMEWORK = Path(os.environ.get("PSXPORT_DIR") or ROOT / "external" / "psxport")
LAUNCHER = FRAMEWORK / "tools" / "port" / "launcher.py"

if os.environ.get("PSXPORT_DIR") and not LAUNCHER.is_file():
    raise SystemExit(f"[run] error: PSXPORT_DIR={FRAMEWORK} is not a psxport checkout")
if not LAUNCHER.is_file():
    subprocess.run(FRAMEWORK_FETCH, cwd=ROOT, check=True)
sys.path[:0] = [str(ROOT / "tools"), str(FRAMEWORK / "tools")]

from run import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
