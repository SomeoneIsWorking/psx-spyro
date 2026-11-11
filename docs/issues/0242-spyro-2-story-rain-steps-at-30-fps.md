# 0242 — Spyro 2's new-game story rain is drawn at the later tick in every in-between

Status: open. Owner: `game/render/field/effect_pools.cpp` (`particlesBetween`, the particle pairing rule).

## Observed
Spyro 2 from boot at the judgement settings (hidden, unpaced), Start/Cross every 100 steps from step 1600
(`scratch/fade2/cut23.py`, `TITLE=spyro2 GS=0x800681C8`), the story's first real-time shot (state 6, handler
`0x800153F8`, dispatcher `0x800155A0(0x1D)`) 40 steps after it begins. The camera and the sheep are still; the rain
streaks fall. Every in-between is composed (`t=0.5 composed=1`), yet each equals the real after it: luma difference to
the later real 0.02-0.05, to the earlier 0.16-0.30 (`between.py`, crop 3000,1000-5400,1700 of 5472x1920), and the
streaks in a crop stand in the in-between exactly where the next real draws them (`s2lb-2460-rain.png`). The run-end
line reads `effects: 5208 of 12398 item draw(s) unpaired`.

## Cause (as far as traced)
The rain is the particle pool (`[0x80066F18]` = `0x801A0440`, 0x20-byte records, kind 1 lines, texture 0x45):
36 records whose head point moves each tick and whose second point is the previous head. Their age byte (+2) falls by
one a tick (`0x1B` to `0x1A`, `pool6.out`). `particlesBetween` pairs a slot only while its age does not fall (plan
`spyro23-replay.md` Pools: "age not falling", which was inferred, not read from a writer), so every rain line is
unpaired and drawn as the later tick.

Not yet found: the writer. A store watch on record 0 (`watch 801A0440 801A0460`, `pool7.log`) reports every store at
`pc=80012CE4 ra=80012CE4`, a `jal 0x800587B4` inside the music controller `FUN_80012CBC`, which is a stale pc, so the
updater that sets the age's direction is unread. Until it is, any other pairing rule is a guess.

## Next
Recover the updater's pc (the store watch's pc is the last synchronised one; a precise store pc is needed), decompile
it, and key the pairing on the slot's spawn as that writer defines it.
