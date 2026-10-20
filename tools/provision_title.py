#!/usr/bin/env python3
"""Resolve and provision one serial-identified Spyro boot executable from a CHD."""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import sys
import tempfile
from collections.abc import Callable, Mapping
from dataclasses import dataclass

import title_identity
from automation.process import ToolError  # noqa: E402  (path set by title_identity)

ROOT = title_identity.ROOT

GENERIC_DISC_KEY = "PSXPORT_DISC"
# Relative to the repository root; the product's catalog names the same root.
PROVISIONING_ROOT = pathlib.Path("scratch") / "assets"


@dataclass(frozen=True)
class ProvisionSpec:
    title: str
    slug: str
    serial: str
    disc_env: str
    cache_dir: pathlib.Path

    @property
    def executable(self) -> pathlib.Path:
        """The provisioned executable, relative to the repository root."""
        return PROVISIONING_ROOT / self.slug / self.serial

    @property
    def env_keys(self) -> tuple[str, ...]:
        """The keys the product reads, in its order: the title's own, then the generic one."""
        return (self.disc_env, GENERIC_DISC_KEY)

    @classmethod
    def from_manifest(cls, slug: str) -> ProvisionSpec:
        manifest = title_identity.load_manifest(slug)
        return cls(
            str(manifest["title"]),
            slug,
            str(manifest["serial"]),
            str(manifest["disc_env"]),
            ROOT / PROVISIONING_ROOT / slug,
        )


SPECS = {slug: ProvisionSpec.from_manifest(slug) for slug in ("spyro1", "spyro2", "spyro3")}


@dataclass(frozen=True)
class ResolvedDisc:
    path: pathlib.Path
    source: str


def _path(value: str, root: pathlib.Path) -> pathlib.Path:
    candidate = pathlib.Path(value).expanduser()
    return candidate if candidate.is_absolute() else root / candidate


def _validate(value: str, source: str, root: pathlib.Path) -> ResolvedDisc:
    path = _path(value, root)
    if not path.is_file():
        raise ToolError(f"{source} names {path}, which is not a file")
    return ResolvedDisc(path.resolve(), source)


def _dotenv(path: pathlib.Path, keys: tuple[str, ...]) -> dict[str, str]:
    if not path.is_file():
        return {}
    values: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ToolError(f"cannot read {path}: {error}") from error
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, value = stripped.split("=", 1)
        key = key.strip()
        if key not in keys:
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value
    return values


def resolve_disc(
    spec: ProvisionSpec,
    argument: str | None,
    *,
    root: pathlib.Path = ROOT,
    environ: Mapping[str, str] = os.environ,
) -> ResolvedDisc:
    if argument is not None:
        return _validate(argument, "CLI argument", root)
    for key in spec.env_keys:
        if environ.get(key):
            return _validate(environ[key], f"${key}", root)
    values = _dotenv(root / ".env", spec.env_keys)
    for key in spec.env_keys:
        if values.get(key):
            return _validate(values[key], f".env ({key})", root)
    dropins = sorted(root.glob("*.chd"), key=lambda path: path.name.casefold())
    if len(dropins) == 1:
        return _validate(str(dropins[0]), "repository-root *.chd drop-in", root)
    if len(dropins) > 1:
        raise ToolError(
            "multiple repository-root CHDs are ambiguous: "
            + ", ".join(path.name for path in dropins)
        )
    raise ToolError(
        "no disc image; pass one explicitly or configure " + " or ".join(spec.env_keys)
    )


def parse_boot_target(path: pathlib.Path) -> str:
    try:
        text = path.read_text(encoding="ascii", errors="replace")
    except OSError as error:
        raise ToolError(f"cannot read extracted SYSTEM.CNF: {error}") from error
    match = re.search(
        r"^\s*BOOT\s*=\s*cdrom:\\+([^;\r\n]+)(?:;1)?\s*$",
        text,
        re.IGNORECASE | re.MULTILINE,
    )
    if match is None:
        raise ToolError("SYSTEM.CNF has no supported BOOT = cdrom:\\...;1 target")
    return pathlib.PureWindowsPath(match.group(1).strip()).name


def extract_boot(discdump: pathlib.Path, disc: pathlib.Path, output: pathlib.Path) -> None:
    result = subprocess.run(
        [str(discdump), str(disc), str(output)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise ToolError(f"discdump failed with exit {result.returncode}: {detail}")


def provision(
    spec: ProvisionSpec,
    disc: pathlib.Path,
    discdump: pathlib.Path,
    *,
    output_dir: pathlib.Path | None = None,
    extract: Callable[[pathlib.Path, pathlib.Path, pathlib.Path], None] = extract_boot,
    identity_check: Callable[[pathlib.Path], list[str]] | None = None,
) -> pathlib.Path:
    """Inspect selected media in fresh staging before consulting or replacing any cache."""
    publish_dir = output_dir or spec.cache_dir
    publish_dir.mkdir(parents=True, exist_ok=True)
    manifest = title_identity.load_manifest(spec.slug)
    executable_name = str(manifest["executable"])
    check_identity = identity_check or (lambda path: title_identity.check(manifest, path))
    with tempfile.TemporaryDirectory(
        prefix=f"{spec.serial}-provision-", dir=publish_dir.parent
    ) as temporary:
        staging = pathlib.Path(temporary)
        extract(discdump, disc, staging)
        system_cnf = staging / "SYSTEM.CNF"
        boot_target = parse_boot_target(system_cnf)
        if boot_target.casefold() != executable_name.casefold():
            raise ToolError(
                f"SYSTEM.CNF boots {boot_target!r}, but {spec.slug} requires {executable_name!r}"
            )
        executable = staging / executable_name
        if not executable.is_file():
            raise ToolError(f"discdump did not extract {executable_name}")
        failures = check_identity(executable)
        if failures:
            raise ToolError(
                f"{spec.serial} identity disagrees on {len(failures)} tracked fact(s)"
            )
        destination = publish_dir / executable_name
        os.replace(executable, destination)
        os.replace(system_cnf, publish_dir / "SYSTEM.CNF")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--title", choices=sorted(SPECS), required=True)
    parser.add_argument("--discdump", type=pathlib.Path, required=True)
    parser.add_argument("disc", nargs="?")
    args = parser.parse_args()
    spec = SPECS[args.title]
    try:
        resolved = resolve_disc(spec, args.disc)
        print(f"[disc] {resolved.source}: {resolved.path}", file=sys.stderr)
        destination = provision(spec, resolved.path, args.discdump)
        print(f"MATCH: SYSTEM.CNF and executable identify {spec.serial}")
        print(f"provisioned {destination.relative_to(ROOT)}")
        return 0
    except (OSError, ToolError) as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
