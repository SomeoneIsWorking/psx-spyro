# 0225 — Bug report replays do not reach the reported screen

Status: open (2026-11-03). Owner: the bug report capture and pad replay (psxport `docs/presentation.md` Bug report capture,
`pad_recording`).

## Observed
The three 2026-10-31 Spyro 1 reports (0222, 0223, 0224) were recorded on `089b610-dirty+psxport-bfc524e3-dirty`.
Replaying each `repro.pad` on a copy of its card, hidden, unpaced or at real speed, is deterministic (two runs give the
same shot) but does not reach the reported screen: `20261031-174318` ends near the Artisans fountain at 3009, not at the
Toasty portal; `20261031-174434` shows the fountain from the other side at 6413; `20261031-174745` stalls at segment 8.
Each report's `psx.png` is also a later frame than its `screen.png` (the camera differs), so the mark cannot be compared.
The C-12 report `20261031-180552` replays to its screen (psx-c12 issue 0010).

## Next
Find where the replays first diverge from the recording (boot pre-roll under the picker is the first suspect) and make
the reference capture take the same frame as the screenshot.
