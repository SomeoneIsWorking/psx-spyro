#!/usr/bin/env python3
"""Get a PNG of the guest framebuffer at a product step, cropped from the REPL's full-VRAM dump.

Usage:
  shot.py 23250                        # -> scratch/screenshots/f23250.png (the framebuffer)
  shot.py 23250 --full                 # the whole 1024x512 VRAM, texture atlas included
  shot.py 23250 --title spyro2 --secs 240
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image

import drive
import provision_title
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

ROOT = drive.ROOT
FRAME_HEIGHT = 240
# VRAM holds the guest's own framebuffer; the widened picture exists only in the host render.
NATIVE_WIDTH = re.compile(r"native picture: aspect=\d+ native_width=(\d+)")


def native_width(log: str) -> int:
    """The guest framebuffer width, from the run's last picture announcement."""
    widths = NATIVE_WIDTH.findall(log)
    if not widths:
        raise ToolError("the run log has no `native picture` line, so the frame width is unknown; pass --width")
    return int(widths[-1])


def variety(image: Image.Image, y0: int, width: int) -> int:
    """Distinct colours over the framebuffer starting at row `y0`."""
    region = image.crop((0, y0, width, min(y0 + FRAME_HEIGHT, image.height)))
    return len(set(region.getdata()))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", type=int, help="product step to capture after (the REPL `run N` unit)")
    ap.add_argument("--full", action="store_true", help="keep the whole 1024x512 VRAM")
    ap.add_argument("--width", type=int, default=None,
                    help="framebuffer width to crop; defaults to the native width the run announces")
    ap.add_argument("--secs", type=int, default=240)
    ap.add_argument("--title", choices=sorted(provision_title.SPECS), default="spyro1")
    ap.add_argument("--executable", type=Path, default=drive.EXECUTABLE)
    a = ap.parse_args()

    out = ROOT / "scratch" / "screenshots"
    out.mkdir(parents=True, exist_ok=True)
    vram = out / f"vram_f{a.step}.png"
    logpath = ROOT / "scratch" / "logs" / f"shot-f{a.step}.log"
    logpath.parent.mkdir(parents=True, exist_ok=True)
    try:
        executable, image = drive.launch_inputs(a.title, a.executable)
        env = drive.environment(a.title)
    except (OSError, ToolError) as refusal:
        sys.exit(f"shot.py REFUSED: {refusal}")
    vram.unlink(missing_ok=True)
    # `end`, not `quit`: quit only detaches the REPL and the run-end reporters never print.
    script = f"run {a.step}\nvram {vram.relative_to(ROOT)}\nend\n"
    print(f"running to step {a.step} …", file=sys.stderr)
    with logpath.open("w") as logf:
        try:
            subprocess.run([str(executable), str(image)], cwd=ROOT, env=env, input=script, text=True,
                           stdout=logf, stderr=subprocess.STDOUT, timeout=a.secs, check=False)
        except subprocess.TimeoutExpired:
            sys.exit(f"the run did not end within {a.secs}s; log {logpath.relative_to(ROOT)}")
    if not vram.is_file():
        sys.exit(f"no VRAM dump produced — did the run reach step {a.step} within {a.secs}s?")
    if a.full:
        print(vram.relative_to(ROOT))
        return 0

    picture = Image.open(vram).convert("RGB")
    try:
        width = a.width if a.width is not None else native_width(logpath.read_text())
    except ToolError as refusal:
        sys.exit(f"shot.py REFUSED: {refusal}")
    fbw = min(width, picture.width)
    top, bottom = variety(picture, 0, fbw), variety(picture, FRAME_HEIGHT, fbw)
    y0 = 0 if top >= bottom else FRAME_HEIGHT
    crop = picture.crop((0, y0, fbw, min(y0 + FRAME_HEIGHT, picture.height)))
    png = out / f"f{a.step}.png"
    crop.save(png, "PNG")
    total = crop.width * crop.height
    counts = {colour: count for count, colour in crop.getcolors(total)}
    lit = total - counts.get((0, 0, 0), 0)
    print(f"{png.relative_to(ROOT)}   (buffer at y={y0}; {top} vs {bottom} distinct colours; "
          f"{100.0 * lit / max(1, total):.2f}% non-black of {crop.width}x{crop.height} = {total} px)")
    print(f"    run log: {logpath.relative_to(ROOT)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
