# 0232 — Spyro 2: the storybook load called from play without its caller's tail faults in departing overlay code

Status: not a player defect (2026-11-04); recorded because the 0230 route first ran into it.
Owner: the route (`scratch/spyro23b/tools/load0230.py`), not the product.

## Observed
`call 80019230 0` from play in Zephyr, with nothing else written, stops the game about 30 steps later:
`[frameloop:error] Spyro 2 stopped ... call 0x8001B140 ... Lightrec fallback refused before interpreter execution:
reason=unsupported_control_flow ... at guest pc=0x80080AF4 ra=0x800704F0`, then the watchdog abort (signal 6).
Reproduced on HEAD: `NOPOKE=1 load0230.py` (`scratch/spyro23b/k1/run.log`; first seen in `r1`..`r5`).

## Cause
Every guest caller of `0x80019230` finishes the load itself right after the call: it clears the stream step
`0x80066EEC` (`sw zero` at `0x80015BC4`, `0x80018414`, `0x80019904`) and the load state `0x80019364` restores the
level `0x80066F90`. The bare call leaves the step as `0x80019230` set it (`0x80019280`), so the load state keeps running
the departing level's overlay (`ra=0x800704F0`, Zephyr's overlay `0x8006D264..0x80084A64`) after the storybook overlay
landed over its top at `0x8007FA64..0x80096264`. `0x80080AF4` is storybook bytes, which Lightrec refuses to run as code.
That refusal is the correct fail-fast.

## Resolution
The route writes what the caller would (`w32 80066eec 0`, `w32 80066f90 2c`); with them the load arrives cleanly.
A player cannot reach the bare call. A `warp` from play through the guest's own load owner would replace the pokes.
