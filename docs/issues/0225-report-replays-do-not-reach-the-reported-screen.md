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
- The first frames after a load draw a 4:3 picture before the 16:9 record takes over (frames 1-5). Not traced to its
  owner. A restore leaves the presentation with no record until the first frame seals; whether
  `psxport runtime/psx/render/record_present.cpp:RenderState::presentRecord` shows the device picture then is unchecked.
- `psx.png` is not the frame `screen.png` shows: `BugReportSession::frameSealed` takes the device's draw area at the
  first seal after the request, and `attachCaptures` shoots the newest present a field later, which at 60 fps is an
  in-between or the next frame.
- A report's camera cannot yet be placed for a matching view: `spyro-at` (`titles/spyro1/core/debug/teleport.cpp`)
  sets the position, but the camera follows Spyro's heading, which the yaw byte at +0xE does not set.
