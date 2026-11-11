# 0246 — Spyro 1 flight results hold the in-between of each reveal frame

Status: open. Owner: psxport `runtime/psx/present/guest_redraw.*` (`GuestRedraw::Skip`) and the executor stop it
rests on (`cpu::resumeGuestToReturn`).

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
A redraw step-over that holds for any number of calls to a callee entry, reached or not: the redraw names the
callees whose work is the real frame's (PlaySound here), and a call to one inside the redraw returns at once. It
belongs to the dispatch the redraw already journals (`SideEffectJournal`), not to the sound native.
