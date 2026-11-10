# SpyroEngine

Native PC products for the original PSX Spyro trilogy; remaining guest instructions run through
psxport's Lightrec integration. Execution policy: the `dynarec-port`, `dynarec-runtime` and
`dynarec-overrides` skills. Also read [`../AGENTS.md`](../AGENTS.md),
[`external/psxport/AGENTS.md`](external/psxport/AGENTS.md) and
[`docs/migration.md`](docs/migration.md) before changing execution architecture. Feature chains are
in [`docs/features.md`](docs/features.md).

## Binary and behavior facts

- Spyro 1 is `SCUS_942.28`, entered at `0x8005B8E0`. The disc boots that executable directly; there
  is no SCEA stub. Its runtime images live inside `WAD.WAD`, and many reuse one guest load address,
  so cache, overrides and original calls key on full image identity. WAD loads invalidate.
- Spyro 1 game main is `0x80012204`; boot ownership includes `0x800127C0` and `0x8001286C`. The 60 Hz
  counter, input, callback root, audio, BIOS events, presentation, pacing and host-turn
  acknowledgement belong to a title-local field owner.
- The recorded dynamic discriminator preserves stage 13 mode 0/1 title presentation and mode 2's
  three-slot save picker (the 900-field run; `PSXPORT_NATIVE_FRAMES=474` fence steps).
- Spyro 2 is `SCUS_944.25`, entry `0x8005478C`, game main `0x80011ADC`, display bootstrap
  `0x80011BBC`, libetc VSync `0x80058EDC`.
- Spyro 3 is `SCUS_944.67`, entry `0x80059444`, game main `0x8001200C`, libetc VSync `0x8005956C`.
- Spyro 2 and 3 run their retail boot prefix through Lightrec and reach gameplay
  (`docs/project-state.md`).

## Title selector

The zero-argument `spyro_port` opens an in-window selector over Spyro 1/2/3 (the framework's
`psx::host`, see `external/psxport/docs/title-host.md`; Spyro supplies `spyro::Catalog`); `./run.sh`
provisions every title whose disc is configured and launches it. `spyro_port <executable>` skips the
selector and runs that one serial-identified executable (`tools/drive.py` uses it). Headless:
`pick <slug>` / `session return` on the control channel; a switch is the same run as a fresh process
(issue 0169).

## Commands

There are no unit tests or gates.

- Build: `CC=clang CXX=clang++ uv run --frozen python external/psxport/tools/port/consumer_build.py --build build/agent --target spyro_port`
- Ghidra lookups: `external/psxport/tools/decomp_pipeline.py --image scratch/assets/<title>/<exe> --target/--callers/--refs/--function-at 0xADDR`.
- Run the product with `tools/drive.py`, `tools/title_route.py`, `tools/shot.py`, or the binary
  through `external/psxport/tools/port/launch_environment.py::agent_environment` (silent,
  `PSXPORT_SHOW_WINDOW=0`: the player's window, never mapped, presenting at real speed; check it in
  `/proc/<pid>/environ`, never set it to 1).
- Judder: drop `PSXPORT_NOPACE`, keep `fps60=1`, reach the scene (`warp <homeworld>/<level>/<arrival>`
  on the control channel, `warp ?` lists) and read the run-end `[playcheck] presents:` line
  (p50/p99/missed).
- Input: `key`, `mouse`, `gamepad`, `touch` and `text` on the control channel inject real SDL events;
  `tap`/`press`/`release`/`hold` press PSX buttons on the same virtual gamepad, through HostInput's
  one PSX-to-SDL binding.
