#!/usr/bin/env python3
"""The guest addresses the C++ owners define, parsed from their headers rather than restated.

Spyro 1's shared globals are this module's attributes; `title_facts(slug)` reads one title's
`titles/<slug>/core/boot_facts.h`. An initialiser the parser cannot evaluate is refused with its line.

    uv run --frozen python tools/guest_globals.py
"""

from __future__ import annotations

import re
from collections.abc import Sequence
from pathlib import Path

from title_identity import ROOT
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

SPYRO1_CORE = ROOT / "titles" / "spyro1" / "core"
HEADERS = (SPYRO1_CORE / "guest_gp.h", SPYRO1_CORE / "guest_globals.h")

DEFINITION = re.compile(
    r"^\s*(?:inline\s+)?constexpr\s+(?:std::)?uint32_t\s+(?P<name>k[A-Za-z0-9_]+)\s*=\s*(?P<value>[^;]+);"
)
INTEGER = re.compile(r"^(?:0[xX][0-9a-fA-F]+|[1-9][0-9]*|0)[uU]?$")
SUM = re.compile(r"^(?P<base>k[A-Za-z0-9_]+)\s*\+\s*(?P<offset>0[xX][0-9a-fA-F]+[uU]?)$")


def _term(text: str, known: dict[str, int], where: str) -> int:
    if INTEGER.match(text):
        return int(text.rstrip("uU"), 0)
    match = SUM.match(text)
    if match and match.group("base") in known:
        return known[match.group("base")] + int(match.group("offset").rstrip("uU"), 16)
    raise ToolError(f"{where}: cannot evaluate the initialiser {text!r}")


def parse(sources: dict[str, str]) -> dict[str, int]:
    """Every `constexpr uint32_t k…` the given headers define, in the order they define them."""
    known: dict[str, int] = {}
    for label, text in sources.items():
        for number, line in enumerate(text.splitlines(), start=1):
            match = DEFINITION.match(line)
            if not match:
                continue
            name = match.group("name")
            value = _term(match.group("value").split("//")[0].strip(), known, f"{label}:{number}")
            if known.get(name, value) != value:
                raise ToolError(f"{label}:{number}: {name} is already 0x{known[name]:08X} here")
            known[name] = value
    if not known:
        raise ToolError(f"no constants in {', '.join(sources)}; the owner's form must have changed")
    return known


def load(headers: Sequence[Path] = HEADERS) -> dict[str, int]:
    missing = [str(path) for path in headers if not path.is_file()]
    if missing:
        raise ToolError(f"missing guest-address owner(s): {', '.join(missing)}")
    return parse({path.name: path.read_text() for path in headers})


def title_facts(slug: str) -> dict[str, int]:
    return load((ROOT / "titles" / slug / "core" / "boot_facts.h",))


_ADDRESSES = load()
globals().update(_ADDRESSES)
__all__ = sorted(_ADDRESSES)


if __name__ == "__main__":
    for name, value in _ADDRESSES.items():
        print(f"{name:34} 0x{value:08X}")
