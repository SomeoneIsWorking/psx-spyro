#!/usr/bin/env python3
"""Format first-party Spyro C++ sources with clang-format."""

from __future__ import annotations

import argparse
import os
import shutil
import site
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = (ROOT / "game", ROOT / "titles")
SOURCE_SUFFIXES = {".cpp", ".h"}


def find_clang_format() -> str:
    configured = os.environ.get("CLANG_FORMAT")
    candidates = [configured, shutil.which("clang-format")]
    candidates.append(str(Path(site.getuserbase()) / "bin" / "clang-format"))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise RuntimeError(
        "clang-format is missing; install it with "
        "`python3 -m pip install --user clang-format` or set CLANG_FORMAT"
    )


def source_files(requested: list[str]) -> list[Path]:
    if requested:
        files = [(ROOT / value).resolve() for value in requested]
        invalid = [path for path in files if not path.is_file() or path.suffix not in SOURCE_SUFFIXES]
        if invalid:
            names = ", ".join(str(path) for path in invalid)
            raise RuntimeError(f"not a supported source file: {names}")
        return sorted(files)

    return sorted(
        path
        for source_root in SOURCE_ROOTS
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()

    try:
        executable = find_clang_format()
        files = source_files(args.files)
    except RuntimeError as error:
        print(f"[format] REFUSED: {error}", file=sys.stderr)
        return 2

    subprocess.run(
        [executable, "--style=file", "-i", *(str(path) for path in files)],
        cwd=ROOT,
        check=True,
    )
    print(f"[format] formatted {len(files)} source files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
