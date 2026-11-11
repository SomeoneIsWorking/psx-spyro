# 0246 — Spyro 1 flight results hold the in-between of each reveal frame

Status: fixed (2026-11-11). Owner: psxport `runtime/psx/present/guest_redraw.*` (`GuestRedraw::applied`) over
`runtime/cpu/side_effect_journal.*` (`SideEffectJournal::withhold`).

## Observed
Flight results (GS 7) are replayed (`DrawSection`, `Flight5` `0x8007B68C` to `0x8007CEC8`), but on each frame that
reveals a row the replay is refused and the interval is shown as drawn: 5 of 2309 intervals in
`scratch/s1states/go.py fres1` (warp `0/5/0`, `w16 0x80075908 0`, `w16 0x8007590A 0`), `the redraw of 0x8007B68C
reached device-write 16-bit @0x1F801C04` at PlaySound `0x80055A78` (ra `0x8007BE08`).

## Cause
`Flight5` plays a row's sound from the draw when `D_8007569C` is below the row (`flight.inc.h`). The replay runs
before the real draw of the frame that advanced `D_80075744`, so it reaches the call first and the SPU write
refuses it. `GuestRedraw::Skip` steps over a callee the real frame already ran, but only one the body reaches every
time and in order: `dispatchTo` stops at one address per stretch (`resumeGuestToReturn` returns at one pc).

## Fix
PlaySound (`0x80055A78`, the native `assignActiveSoundSlot`) returns 0 in v0 (0x7F on its size-limit rejection),
a varying v1, and writes the chosen slot through a3 when a3 is set. Flight5's five calls (returns `0x8007BE08`,
`0x8007C004`, `0x8007C1FC`, `0x8007C3F8`, `0x8007C5B4`) pass a3 = 0, overwrite v0 with the row number at once
(`addiu v0, zero, N` before `sw v0, D_8007569C`) and set a0 to a3 again before the next call, so they read nothing
back. The real draw plays the sound; a replay only needs the call gone. `GuestRedraw::applied` names host services
whose work is the real frame's: the Discard journal withholds each one wherever the run reaches it, and the
dispatch returns at the caller's ra with the registers as the call found them (`dispatchGuestHostService`). It
aborts at a callee that is guest code. `DrawSection` names PlaySound for the flight results draw.

## Evidence
`fres3` (same route): 2314 intervals replayed, 0 refused (was 5), 0 of 27792 HUD records unpaired; in-betweens
0.26 / 0.10 and 0.31 / 0.15 to their neighbours. fps60 on/off fielddigest identical over 5460 fields (`fleak30`,
`fleak31`).
