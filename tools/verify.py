#!/usr/bin/env python3
"""Run Spyro's build, C++ quality checks, test registration, complete CTest suite, and pin gate."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import run


def verify_cpp_quality(build: Path, jobs: int) -> None:
    """Check the first-party code compiled by the current product and its tests."""
    entries = json.loads((build / "compile_commands.json").read_text())
    sources = sorted({
        Path(entry["file"]).resolve()
        for entry in entries
        if Path(entry["file"]).resolve().is_relative_to(run.ROOT)
        and Path(entry["file"]).resolve().relative_to(run.ROOT).parts[0]
        in {"game", "titles", "tests"}
    })
    if not sources:
        raise run.Refusal("compile database contains zero first-party translation units")
    files = sorted(set(sources) | {path.with_suffix(".h") for path in sources if path.with_suffix(".h").is_file()})
    for path in files:
        lines = len(path.read_text().splitlines())
        if lines > 1200:
            raise run.Refusal(f"{path.relative_to(run.ROOT)}: {lines} lines exceeds the 1200-line structure limit")
    run.command([sys.executable, run.ROOT / "tools/format.py", "--check", *files])
    # clang-tidy is single-threaded per process; one shard per job keeps the whole tree checked
    # without serialising hundreds of translation units through one process.
    shards = [sources[index::jobs] for index in range(min(jobs, len(sources)))]
    with ThreadPoolExecutor(max_workers=len(shards)) as pool:
        for future in [pool.submit(run.command, ["clang-tidy", "--quiet", "-p", build, *shard]) for shard in shards]:
            future.result()
    print(f"[verify] C++ quality: {len(sources)} translation units, {len(files)} source/header files")


def verify_every_test_is_registered() -> None:
    """Refuse a test source that no CMake target compiles.

    Measured 2026-09-22: 28 of 71 `tests/test_*.cpp` were compiled by nothing at all. They were not
    failing — they were not running, which is worse, because a suite that silently shrinks reports
    the same green as one that covers everything. One of them had asserted a refusal that had been
    removed from the product months earlier and would not even compile. A test that is not in a
    target is not a test, so the gate now says so by name.
    """
    cmake = (run.ROOT / "CMakeLists.txt").read_text()
    built = set(re.findall(r"\btests/((?:\w+/)?test_\w+)\.cpp", cmake))
    for items in re.findall(r"foreach\(test_name IN ITEMS ([^)]*)\)", cmake):
        built |= {f"test_{name}" for name in items.split()}
    # A title's tests: `<title>/<test>` builds tests/<title>/test_<test>.cpp.
    for items in re.findall(r"foreach\(test_path IN ITEMS ([^)]*)\)", cmake):
        built |= {f"{title}/test_{name}" for title, name in (item.split("/") for item in items.split())}
    tests = run.ROOT / "tests"
    sources = {path.relative_to(tests).with_suffix("").as_posix() for path in tests.rglob("test_*.cpp")}
    orphans = sorted(sources - built)
    if orphans:
        raise run.Refusal(
            f"{len(orphans)} of {len(sources)} test sources are compiled by no target: "
            + ", ".join(f"tests/{name}.cpp" for name in orphans)
        )
    print(f"[verify] test registration: all {len(sources)} test sources are built by a target")


def verify(jobs: int) -> None:
    compiler_options = run.preflight()
    psxport = run.sync_framework()
    run.sync_submodules(psxport)
    run.configure(
        run.ROOT,
        run.MAINTAINER_BUILD,
        compiler_options,
        f"-DPSXPORT_DIR={psxport}",
        build_testing=True,
    )
    run.command(["cmake", "--build", run.MAINTAINER_BUILD, "-j", str(jobs)])
    verify_cpp_quality(run.MAINTAINER_BUILD, jobs)
    verify_every_test_is_registered()
    run.command(
        ["ctest", "--test-dir", run.MAINTAINER_BUILD, "--output-on-failure"]
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    try:
        verify(args.jobs)
    except (OSError, run.Refusal) as error:
        print(f"[verify] REFUSED: {error}", file=sys.stderr)
        return 2
    print("[verify] PASS: build, C++ quality, test registration, complete CTest, and psxport pin")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
