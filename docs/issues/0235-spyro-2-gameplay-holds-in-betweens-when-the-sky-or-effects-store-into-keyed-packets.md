# 0235 — Spyro 2 gameplay holds in-betweens when the sky or effects store into keyed packets

Status: closed (2026-11-04, spyro2e): fixed in psxport `runtime/psx/gpu/ordering_table.cpp`.
Owner: psxport `submitOrderingTable`, which names a keyed packet's `mixedOwner`; not the field producer.

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

## Cause
`psxport runtime/psx/gpu/ordering_table.cpp:submitOrderingTable` marked a keyed packet as overwritten when any of its
words replaced a word of *any* scope opened since the last draw list, not only its own scope's. The S2/S3 terrain
drawer, under its producer scope `0x80023BB4`, leaves retail's stores at primitives it then rejects past the cursor it
publishes (for one, `guest_terrain_split.cpp:pieceFacesCamera` stores the first screen word at `primitive + 8`). The
next drawer allocates its packet at that cursor: the effects sprites (`0x8004A8D4`, word `+0xC`) right after the
terrain, the sky (`0x80029170`, word `+8`) after the HUD. The flatten keys that packet, the packet's word carries the
terrain scope as replaced, and the verdict holds the interval. Traced with a probe at the dispatcher, sky and flatten
(replaced scope `0x80023BB4`, serial of the same frame's terrain, in every hit). Which levels and stretches it hit
followed only from where the terrain's last rejected primitive fell against the next drawer's packet words.

## Fix
An owned packet is mixed only when the replaced word was its own scope's (`scopeKey(replaced) == key`); an unowned
packet keeps the scope its writer stored over. psxport `docs/presentation.md` (Verdict) records the rule.

## Shown
Same route, same binary settings, hidden and unpaced (`scratch/spyro2e/fixed/run.log`, blocks in
`fixed/block{0,1,2}.log`; `scratch/spyro2e/tools/sim60held.py` simulates the 60.00 Hz display and counts vblanks that
repeat a held in-between):

| level | held intervals before | after | held repeats on the 60.00 Hz display after |
|---|---|---|---|
| Hurricos `0/4/0` | 0 of 59 | 0 of 59 | the fall after the ledge (one-field frames, no in-betweens), as before |
| Skelos Badlands `1/1/0` | 41 of 200 | 2 of 200 | 10666, 10669: terrain `producer-refused` after a disc landing (load hold) |
| Mystic Marsh `2/1/0` | 26 of 200 | 0 of 200 | none |

No `Overwrote` row in the fixed run. In the old held windows (`scratch/spyro2e/half3/run.log` and `*_strip.png`, presents at Skelos
10915-10918 and Mystic Marsh 15545-15548) every t=0.5 present composes; Mystic Marsh's pan lands at 0.47 of the
real-to-real pan, Skelos's in-between splits the picture difference 3.37/3.30 (its pan is under 2 px, too small to
measure). Pacing was not judged (host load above 4).
