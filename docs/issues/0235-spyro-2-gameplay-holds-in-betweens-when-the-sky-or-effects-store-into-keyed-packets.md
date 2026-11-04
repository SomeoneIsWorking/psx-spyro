# 0235 — Spyro 2 gameplay holds in-betweens when the sky or effects store into keyed packets

Status: open (2026-11-04, spyro23d): found, owner named, cause not traced.
Owner: the field producer `game/render/field/field_redraw.cpp` (keys at the flatten `0x8001B2A8`) against the guest
writers the owner journal names: the sky drawer `0x80029170` (`game/render/field/guest_sky_drawer.cpp`) and the
effects pass sprites `0x8004A8D4`.

## Observed
Hidden, unpaced, judgement settings, camera turning (Left held 300 steps, then Up 300). The 60 fps in-betweens
compose most of the time, but whole stretches of gameplay show each logic frame twice (held intervals, 30 Hz
motion):

| level (warp) | intervals | held | longest held stretch |
|---|---|---|---|
| Hurricos (`0/4/0`) | 59 before Spyro walks off the ledge | 0 | - |
| Skelos Badlands (`1/1/0`) | 200 | 41 | fences 10912-11026, 39 logic frames (1.3 s) |
| Mystic Marsh (`2/1/0`) | 200 | 26 | fences 15542-15599, 20 logic frames |

Where intervals compose, the in-between lands halfway: the background pan of each t=0.5 present is 0.49-0.51 of
its real-to-real pan in all three levels (`scratch/spyro23d/halfway.txt`).

## Evidence
Route: `scratch/spyro23d/tools/seqlong.py spyro2 <out> 600 0/4/0 1/1/0 2/1/0` (one process, warps in turn; the
Skelos and Mystic Marsh fences above belong to that route). Log: `scratch/spyro23d/L_spyro2/run.log`, blocks split
in `L_spyro2/block{0,1,2}.log`. The owner journal rows next to the held stretches:

- Skelos 10666-10669: `IntervalHeld/producer-refused`, terrain producer `0x80023BB4`, bin 14.
- Skelos 10912-11026: `Overwrote` writer `0x80029170` (sky), producer `0x8001B2A8`, bin 513, GP0 `0x30`.
- Mystic Marsh 15491: `Overwrote` writer `0x8004A8D4` (effects sprites), producer `0x8001B2A8`, bin 12, GP0 `0x32`
  (also logged at `warp 0/0/0` bin 11 and `warp 0/5/0` bin 10).

`Overwrote` means a keyed packet carries a store from another writer (`mixedOwner`, psxport
`ownership_verdict.cpp`), so the composer holds the interval rather than render a packet it cannot reproduce.

## Next
Trace on HEAD with `debug compose,presentseq` at Skelos fence 10912: which packet the sky drawer stores into after
the flatten keyed it (the redraw's own sky packets, placed by `placeSky`, or a pool word reused from the previous
field), then fix at the field producer so the sky and effects packets it keys are the ones those writers fill.
`game/render/field/` is shared with the depth lanes, which had it mid-edit in this pass.
