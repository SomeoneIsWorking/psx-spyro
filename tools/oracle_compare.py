#!/usr/bin/env python3
"""Compare the Spyro 1 Lightrec product against psxport's independent Beetle full-console
reference at title-owned state checkpoints (psxport docs/oracle.md, tools/oracle/compare.py).

    uv run --frozen python tools/oracle_compare.py --bios ../SCPH1001.BIN
    uv run --frozen python tools/oracle_compare.py --bios ../SCPH1001.BIN --policy demo

Two title policies share one lens (tools/oracle_spyro1.py): `artisans` drives the New Game menu into
the homeworld, and `demo` hands the title screen to the attract demo so a LEVEL ENTRY is compared
(tools/oracle_spyro1_demo.py, docs/issues/0114). The launch inputs and environment are tools/drive.py's.
"""

from __future__ import annotations

import sys
from pathlib import Path

import drive
import provision_title
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

ROOT = drive.ROOT
sys.path.insert(0, str(ROOT / "external" / "psxport" / "tools" / "oracle"))

import compare  # noqa: E402
import oracle_spyro1  # noqa: E402
import oracle_spyro1_demo  # noqa: E402

OUT_DIR = ROOT / "scratch" / "oracle"
DEFAULT_BIOS = ROOT.parent / "SCPH1001.BIN"

# One lens, two routes. `artisans` drives the New Game menu into the homeworld; `demo` hands the
# title screen to the attract demo and compares a LEVEL ENTRY, which the menu route cannot reach
# (docs/issues/0114, and oracle_spyro1_demo's docstring). They are separate modules rather than one
# route with flags because checkpoints are a single ordered chain from boot: the demo's timeout arm
# lives in TSM_Init, which a route that has already answered the menu never returns to.
ROUTES = {"artisans": oracle_spyro1, "demo": oracle_spyro1_demo}


def main() -> int:
    parser = compare.build_parser(__doc__, DEFAULT_BIOS)
    parser.add_argument("--executable", type=Path, default=drive.EXECUTABLE)
    parser.add_argument("--policy", choices=sorted(ROUTES), default="artisans",
                        help="which title policy to compare: 'artisans' (New Game into the homeworld) "
                             "or 'demo' (the no-input attract demo, which crosses a level entry)")
    args = parser.parse_args()
    title = ROUTES[args.policy]
    try:
        executable, image = drive.launch_inputs("spyro1", ROOT / args.executable)
        environment = drive.environment("spyro1")
    except (OSError, ToolError) as refusal:
        print(f"REFUSED: {refusal}", file=sys.stderr)
        return 2
    disc = Path(environment[provision_title.SPECS["spyro1"].disc_env])
    environment.update(compare.product_env(args))
    product = compare.Product(executable, image, environment, ROOT, disc)
    return compare.run(title, product, args, OUT_DIR)


if __name__ == "__main__":
    raise SystemExit(main())
