# 0254 — Spyro 3: an isolated 33.5 ms present stall, and the outline fields built when the Atlas opens

Status: open. The isolated stall did not reproduce; the field build delays one present every time.
Owners: psxport `runtime/psx/present/outline_text.cpp` (`OutlineText::Builder::drew`), `game/core/boot_prefix_frame_driver.cpp`
(the perf meter's frames).

## Observed
A paced run on 2026-11-14 (`scratch/rt1114/s3c`: `warp 0/1/0` to Sunny Villa, Select twice for the Atlas, then
`pshot`s) showed one t = 1 present at fence 1383, six presents after the Atlas closed, shown 33.5 ms past its due
time (interval 50.0 ms), after an on-time in-between. Nothing was logged between the two, and the guest call before
it cost what its neighbours did (9988 guest cycles).

## Reproduction, 2026-11-14
Four paced runs, hidden window, `ires=0`, `fps60=1`, 3840x2160, the same route without `pshot`
(`scratch/stall/cmds.txt`, `tools/paced.py`, `tools/gaps.py`; host CPU sampled each second, `load<n>.txt`):

| run | presents p50 / p99 | missed | late presents after arrival (fence > 1035) |
|---|---|---|---|
| r1 | 16.75 / 18.00 ms | 7 | 1264 (+14.3 ms) |
| r2 | 16.75 / 22.75 ms | 18 | 1264 (+14.4 ms) |
| r3 | 16.75 / 17.25 ms | 6 | 1264 (+13.9 ms) |
| r4 | 16.75 / 30.00 ms | 25 | 1264 (+14.0 ms); 1187-1206, in the second another project's C# compiler ran 10 cores |

None showed a late present at or near fence 1383 or an unexplained one elsewhere. The host was not quiet: a Godot
process held about one core through r1-r3. The isolated stall stays unexplained and is recorded here, not changed.

## The late present at Atlas open
Fence 1264 is late in all four runs and in the three runs of 2026-11-14 morning (`scratch/rt1114/s3`, `s3b`,
`s3c`). Just before it, every time: `[outline] atlas 3: 3 fonts, 14 fields drawn`. Opening the Atlas draws its
font's glyphs for the first time; `OutlineText::Builder::drew` reads each new glyph's bitmap from VRAM and draws its
multi-channel distance field (msdfgen) on the presenting thread, inside the sealed frame, so the in-between due next
is shown about 14 ms late: one missed vblank. Atlas 4 (10 fields, after fence 1271) and atlases 1-2 (12 and 2 fields
at arrival, fences 1014 and 1030) are the same work at other first sightings. The fields are cached by glyph and
drawn once a run; none is drawn twice. The work is needed. Its place on the present path is the problem.

Proper fix (psxport, not made): draw a font's fields away from the present that first shows its glyphs, either on a
worker with the bitmap glyph shown until its field is ready, or for every named glyph of a font when its page first
lands in VRAM. Either changes `OutlineText`'s contract (psxport plan 0207), so the choice is the user's.

## Meter
`PSXPORT_DEBUG=perf` counted nothing for Spyro 2 and 3 (`0 logic frames metered`, `0 record presents`): their
frame driver never opened a frame. `BootPrefixFrameDriver::stepFrame` now meters each main-loop step as a frame,
update turns as `game-logic`, draw turns and the step's present as `present`. r4: 1236 frames, `frame` p50 249.6
p99 268.4 max 1147.3 Minsn, `present` p50 186.6 max 447.0, `game-logic` p50 8.5 max 700.3; 2453 record presents,
draws p50 1119 max 2296.
