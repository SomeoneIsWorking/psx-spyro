#!/usr/bin/env python3
"""title_conversation.py — walk into a Spyro 2 conversation and prove its text box draws glyphs.

    uv run --frozen python tools/title_conversation.py --title spyro2 --shot-dir scratch/play/spyro2-talk

WHY THIS EXISTS. `title_route.py` walks AWAY from the first conversation because an early capture of it
showed dark rounded rectangles and no text. The cause was not a missing text path: the guest reveals a
conversation's words one character at a time after the box opens (and the box shrinks away empty at the
end), so a capture taken while the box is opening or closing has a box and no glyphs, and the same run
captured later shows the whole speech. This route makes that a measured fact instead of an impression:
it walks forward until the guest's own game state says "conversation", then photographs the box every
SAMPLE steps and counts glyph-coloured pixels in the box body for each photograph.

WHAT COUNTS AS DRAWN TEXT. The state must have been the title's conversation state; at least MIN_CAPTURES
captures must have been taken inside it; and the body must hold at least MIN_GLYPH_PIXELS glyph pixels in
its fullest capture, with the first capture (box just opened) holding fewer. A run where the box opens and
no glyph ever appears is refused by name, which is the failure the early capture looked like. The glyph
colours are the two font colours measured from the shipped captures (not asserted by the guest), counted by
exact match so Spyro's orange wings and the HUD cannot be mistaken for text; the body region excludes the
speaker-name strip above the box. Every capture prints its count against the region's pixel total.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

import drive
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from port.present_geometry import Unreadable, read_image  # noqa: E402
from title_profile import TitleProfile
from title_route import (
    EXIT_OK,
    EXIT_REFUSED,
    SETTLE,
    STEP,
    Evidence,
    LevelIdentity,
    RoutePort,
    open_port,
    read_level,
    reach_playing,
)

GLYPH_COLOURS = frozenset({(176, 136, 8), (192, 144, 8)})  # the two font colours, measured from the captures
WALK_BUDGET = 200  # product steps of walking allowed to open the conversation (Spyro 2 opens it in ~20)
SAMPLE = 15  # steps between captures inside the conversation
CAPTURE_BUDGET = 450  # steps allowed inside the conversation before the route gives up
MIN_CAPTURES = 6
MIN_GLYPH_PIXELS = 1000
# The box body as fractions of the frame, excluding the speaker-name strip that sits above the box.
BODY_X = (0.04, 0.96)
BODY_Y = (0.58, 0.89)


@dataclass(frozen=True)
class Capture:
    path: Path
    glyph_pixels: int
    region_pixels: int


@dataclass
class ConversationEvidence:
    arrival_step: int = 0
    level: LevelIdentity | None = None
    opened_after: int = 0
    captures: list[Capture] = field(default_factory=list)

    def peak(self) -> int:
        return max((c.glyph_pixels for c in self.captures), default=0)


def count_glyph_pixels(width: int, height: int, rgb: bytes) -> tuple[int, int]:
    """(glyph-coloured pixels, pixels examined) over the conversation box body."""
    x0, x1 = int(width * BODY_X[0]), int(width * BODY_X[1])
    y0, y1 = int(height * BODY_Y[0]), int(height * BODY_Y[1])
    glyphs = 0
    for y in range(y0, y1):
        row = y * width * 3
        for x in range(x0, x1):
            offset = row + x * 3
            if (rgb[offset], rgb[offset + 1], rgb[offset + 2]) in GLYPH_COLOURS:
                glyphs += 1
    return glyphs, (x1 - x0) * (y1 - y0)


def capture(port: RoutePort, path: Path) -> Capture:
    port.shot(str(path))
    try:
        width, height, rgb = read_image(path)
    except Unreadable as error:
        raise ToolError(str(error)) from error
    glyphs, region = count_glyph_pixels(width, height, rgb)
    return Capture(path, glyphs, region)


def _require_dialogue(entry: TitleProfile) -> tuple[int, str]:
    if entry.state_dialogue is None or entry.dialogue_button is None:
        raise ToolError(f"{entry.label}: no conversation state has been observed for this title, so there is no route")
    return entry.state_dialogue, entry.dialogue_button


def converse(port: RoutePort, entry: TitleProfile, shot_dir: Path, root: Path = Path(".")) -> ConversationEvidence:
    """Reach gameplay, walk until the guest's state is the conversation, and photograph the box."""
    state_dialogue, button = _require_dialogue(entry)
    evidence = ConversationEvidence()
    seen = Evidence()
    evidence.arrival_step = reach_playing(port, entry, seen)
    port.run(SETTLE)
    if entry.level is not None:
        evidence.level = read_level(port, entry.level)
    port.press(button)
    walked = 0
    while port.gamestate() != state_dialogue:
        port.run(STEP)
        walked += STEP
        if walked >= WALK_BUDGET:
            port.release(button)
            raise ToolError(f"{entry.label}: {button} for {WALK_BUDGET} steps never opened a conversation (state {state_dialogue})")
    port.release(button)
    evidence.opened_after = walked
    elapsed = 0
    while port.gamestate() == state_dialogue and elapsed < CAPTURE_BUDGET:
        port.run(SAMPLE)
        elapsed += SAMPLE
        evidence.captures.append(capture(port, root / shot_dir / f"talk{len(evidence.captures):02d}.ppm"))
    return evidence


def verdict(entry: TitleProfile, evidence: ConversationEvidence) -> None:
    """Refuse, by name, a conversation whose box never drew text."""
    if len(evidence.captures) < MIN_CAPTURES:
        raise ToolError(f"{entry.label}: only {len(evidence.captures)} captures inside the conversation (< {MIN_CAPTURES})")
    if evidence.peak() < MIN_GLYPH_PIXELS:
        raise ToolError(
            f"{entry.label}: the conversation box opened but its fullest capture holds {evidence.peak()} glyph pixels "
            f"(< {MIN_GLYPH_PIXELS}): the box draws and the text does not"
        )
    if evidence.captures[0].glyph_pixels >= evidence.peak():
        raise ToolError(f"{entry.label}: the first capture already holds the peak, so the text was not revealed over time")


def report(entry: TitleProfile, evidence: ConversationEvidence) -> str:
    lines = [
        f"{entry.label}: conversation opened {evidence.opened_after} steps into the walk "
        f"(arrival at step {evidence.arrival_step})",
    ]
    if evidence.level is not None:
        lines.append(f"  level {evidence.level.name} (id {evidence.level.level_id})")
    lines.append(f"  glyph pixels per capture, every {SAMPLE} steps, of {evidence.captures[0].region_pixels} body pixels:")
    lines += [f"    {c.path.name}: {c.glyph_pixels}" for c in evidence.captures]
    lines.append(f"  peak {evidence.peak()} (>= {MIN_GLYPH_PIXELS} required), first {evidence.captures[0].glyph_pixels}")
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--title", choices=("spyro2", "spyro3"), required=True)
    parser.add_argument("--executable", type=Path, default=drive.EXECUTABLE)
    parser.add_argument("--log", type=Path, default=Path("scratch/play/title_conversation.log"))
    parser.add_argument("--shot-dir", type=Path, default=Path("scratch/play/talk"))
    args = parser.parse_args(argv)
    try:
        entry, port = open_port(args.title, args.executable, args.log)
        _require_dialogue(entry)
        try:
            evidence = converse(port, entry, args.shot_dir, drive.ROOT)
        finally:
            port.end()
        print(report(entry, evidence))
        verdict(entry, evidence)
    except (OSError, ToolError) as refusal:
        print(f"REFUSED: {refusal}", file=sys.stderr)
        return EXIT_REFUSED
    return EXIT_OK


if __name__ == "__main__":
    raise SystemExit(main())
