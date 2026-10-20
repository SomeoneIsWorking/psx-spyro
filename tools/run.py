#!/usr/bin/env python3
"""Provision every Spyro title whose disc is configured, build spyro_port, and open its title selector."""

from __future__ import annotations

import os
from collections.abc import Sequence
from pathlib import Path

import provision_title
from automation.process import ToolError  # noqa: E402  (path set by title_identity)
from port import launcher  # noqa: E402
from port.launch_environment import player_environment  # noqa: E402

ROOT = provision_title.ROOT
# Content identity hashes with OpenSSL.
OPENSSL = launcher.Dependency("OpenSSL development files",
                              launcher.install_commands("openssl-devel", "libssl-dev", "brew install openssl@3",
                                                        "vcpkg install openssl"), pkg_config="openssl")


def provision_titles(discdump: Path) -> dict[provision_title.ProvisionSpec, Path]:
    """Every title whose disc resolves and authenticates; one missing disc does not stop the others."""
    discs = {}
    for spec in provision_title.SPECS.values():
        try:
            disc = provision_title.resolve_disc(spec, None).path
            launcher.say(f"{spec.slug} disc: {disc}")
            provision_title.provision(spec, disc, discdump)
        except (OSError, ToolError) as refusal:
            launcher.say(f"{spec.slug} not provisioned: {refusal}")
            continue
        discs[spec] = disc
    if not discs:
        raise ToolError("no Spyro title could be provisioned; configure a disc in .env")
    return discs


def launch(prepare_only: bool) -> None:
    launcher.preflight((OPENSSL,))
    discs = provision_titles(launcher.build_discdump(ROOT, os.environ))
    build = launcher.build_product(ROOT, "spyro_port", os.environ)
    if prepare_only:
        launcher.say(f"built and ready; titles provisioned: {', '.join(spec.slug for spec in discs)}")
        return
    env = player_environment(os.environ, product="spyro", settings=ROOT / "tools" / "shipping_settings.ini")
    env.setdefault("PSXPORT_DEBUG_SERVER", "1")
    env.setdefault("PSXPORT_FULLSCREEN", "1")
    for spec, disc in discs.items():
        env[spec.disc_env] = str(disc)
    launcher.exec_product(build / "bin" / "spyro_port", [], env, ROOT)


def main(argv: Sequence[str] | None = None) -> int:
    args = launcher.argument_parser(__doc__, disc=False).parse_args(argv)
    return launcher.main(lambda: launch(args.prepare_only))


if __name__ == "__main__":
    raise SystemExit(main())
