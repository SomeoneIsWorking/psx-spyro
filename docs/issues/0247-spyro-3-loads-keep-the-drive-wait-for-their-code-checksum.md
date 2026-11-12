# 0247 — Spyro 3 loads keep the drive wait for their code checksum

Status: open (2026-11-12). Owner: `titles/spyro3/core/runtime.cpp` (`kCdCompletion`).

## Observed
With its stock read completions due at once (`psx::cd::ReadTiming::AtOnce`, as Spyro 2 has), Spyro 3's loading
screen never ends: `warp 0/1/0` enters state 5 at step 1018 and the loader stage `0x8006C518` stops at 7 from
step 1051 to the end of the run at 1500 (`scratch/frameloop/run/s3_trace3.log`).

## Cause
`0x8002CA50`, the loading screen's per-frame loader (called from `0x80058778` sub-state 1), runs a stage only when
`FUN_800503f8` reports no read in flight; while one is in flight and the stage is 2..7 it checksums 0x1400 bytes
of code (cursor `0x8006C6B8` from `0x8000D8CF` to `0x8006085E`, then `0x8007020E` to `0x80076FA6`, then
`0x8007B6E4`; sum `0x8006C6B4`). Stage 7 (`0x800750CC` in the loaded overlay) advances only once the cursor is
`0x8007B6E4`, and stage 8 zeroes a random word of `0x8002AB38` unless the sum is 0. With no read ever in flight
the checksum never runs, so stage 7 waits forever. The drive time is what the checksum runs in.

## Not done
Spyro 3 keeps `ReadTiming::Drive`. Removing its load wait needs the checksum's frames without the read's: no
faithful terminal is known, so its loads stay as retail times them (state row S035).
