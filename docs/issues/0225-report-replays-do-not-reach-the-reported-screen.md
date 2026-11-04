# 0225 — Bug report replays do not reach the reported screen

Status: fixed for new reports (2026-11-03); the three 2026-10-31 reports stay unreproducible. Owner: the bug report capture and pad replay (psxport `docs/presentation.md` Bug report capture,
`pad_recording`).

## Observed
The three 2026-10-31 Spyro 1 reports (0222, 0223, 0224) were recorded on `089b610-dirty+psxport-bfc524e3-dirty`.
Replaying each `repro.pad` on a copy of its card, hidden, unpaced or at real speed, is deterministic (two runs give the
same shot) but does not reach the reported screen: `20261031-174318` ends near the Artisans fountain at 3009, not at the
Toasty portal; `20261031-174434` shows the fountain from the other side at 6413; `20261031-174745` stalls at segment 8.
Each report's `psx.png` is also a later frame than its `screen.png` (the camera differs), so the mark cannot be compared.
The C-12 report `20261031-180552` replays to its screen (psx-c12 issue 0010).

## Cause
Pad replay is deterministic on one build but not across builds: the owner-journal timeline of `20261031-174318`
matches the recording through field 2370 (end of the Nestor rescue), then Spyro's route diverges in gameplay. The
recording build (`089b610-dirty+bfc524e3-dirty`) cannot be rebuilt, so a pad recording alone cannot carry a report
to its screen (`psxport runtime/psx/debug/bug_report_session.cpp:BugReportSession::attachCaptures`).

## Fix
A report also saves the machine state at the reported field (`state.bin`) and its reproduction loads it
(`PSXPORT_LOAD_STATE`, shot at frame 30). Spyro 1's frame driver saves its native state (`spyro1.frame-driver`), and a
state carries the active code images (`images` section), so a fresh process dispatches the loaded WAD overlays.
Shown: replay to 3009, B report, load of its `state.bin` in a new process: the frame-30 shot matches the report's
`screen.png` (`scratch/depth/runs/state-r1c-load/sheet.png`).

## Remaining
- The first frames after a load show the 4:3 device picture before the 16:9 record takes over. Traced on the
  `r0224` state (`scratch/depth3/runs/load43`, `load43b` with `PSXPORT_DEBUG=recordcheck`, sheet
  `scratch/depth3/load43.png`): frames 1-2 are 4:3, frame 3 on is 16:9. The state carries VRAM but no frame record. The
  first record after the restore (seq 1) has no entries and is resynced from VRAM, so
  `record_present.cpp:RenderState::presentRecord` shows the restored device picture at its 4:3 width until the
  first guest frame seals (seq 2). Fix: save the last sealed record with the state, or hold the restored present
  until the first record seals. Seq 3's 4:3 check also reports 83,196 pixels where record and device differ, which is
  not traced.

## Fixed since
- `psx.png` and `screen.png` are one frame: `BugReportSession::presented` shoots the first whole present of the sequence
  `frameSealed` captured (from `DisplayPresenter::showScheduled`), and a report with none in 4 fields says so instead of
  attaching a mismatched `psx.png` (`scratch/depth3/r0224-sheet.png`).
- `spyro-at x y z heading` sets Spyro's heading (target and current words at +0xCC and +0x11C), and `where` prints the
  position and heading. The +0x0C bytes it printed before are his body rotation, which the camera does not follow.
