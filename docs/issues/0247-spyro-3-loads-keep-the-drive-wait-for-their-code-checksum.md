# 0247 — Spyro 3 loads keep the drive wait for their code checksum

Status: fixed (2026-11-12). Owner: `titles/spyro3/core/loader_checksum.cpp`, `titles/spyro3/core/runtime.cpp` (`kCdCompletion`).

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

The checksum is one CRC-like sum (each byte at cursor + 0x473E XORed in at bit 15, eight shifts with `0x400280`
folded in at bit 23); in the first range the cursor steps 1..4 bytes by the sum's bits 7..8. Its cursor and sum
carry over between calls and the first range hands over to the second inside one call, so how the 0x1400-step
slices fall does not change the result: about 33 in-flight calls reach the same `0x8007B6E4` and sum as one
call that runs every slice. `FUN_800503F8` is the read poll itself (it starts the next read), so nothing else
may ask it.

## Resolution
Spyro 3's stock reads are due at once (`ReadTiming::AtOnce`). A native override at `0x8002CA50`
(`spyro3::installLoaderChecksum`) runs, when the stage is 2..7 (retail's own `stage - 2 < 6`) and the cursor is
not at its end, every slice the in-flight calls would have run, from the same bytes, then the original loader.
A changed code byte still fails stage 8's test and sabotages as retail does.

`warp 0/1/0` (`scratch/s3load/`): state 5 entry to the first gameplay frame 380 steps / 663 fields with drive
timing, 75 steps / 131 fields now; stage 8 takes its success branch (cursor and sum back to 0) and
`0x8002AB38`, `0x8002AAFC`, `0x8002A7B4`, `0x8002A834` match the executable byte for byte at step 1500; the boot
returns after 677 fields (715 with drive timing). The level plays (`s3_1500.png`).
