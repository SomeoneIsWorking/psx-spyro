# 0242 — Spyro 2's new-game story rain is drawn at the later tick in every in-between

Status: fixed. Owner: `game/render/field/effect_pools.cpp` (`particlesBetween`, the particle pairing rule; `game/core/draw_motion.cpp` `installEffectSpawns`).

## Observed
Spyro 2 from boot at the judgement settings (hidden, unpaced), Start/Cross every 100 steps from step 1600
(`scratch/fade2/cut23.py`, `TITLE=spyro2 GS=0x800681C8`), the story's first real-time shot (state 6, handler
`0x800153F8`, dispatcher `0x800155A0(0x1D)`) 40 steps after it begins. The camera and the sheep are still; the rain
streaks fall. Every in-between is composed (`t=0.5 composed=1`), yet each equals the real after it: luma difference to
the later real 0.02-0.05, to the earlier 0.16-0.30 (`between.py`, crop 3000,1000-5400,1700 of 5472x1920), and the
streaks in a crop stand in the in-between exactly where the next real draws them (`s2lb-2460-rain.png`). The run-end
line reads `effects: 5208 of 12398 item draw(s) unpaired`.

## Cause
The rain is the particle pool (`[0x80066F18]` = `0x801A0440`, 0x20-byte records, kind 1 lines, texture 0x45). Its
updater is the story overlay's `0x80075024`, pool loop `0x80075670..0x80075838` (`scratch/rain/f75024.s`): each tick it
moves both points by head minus tail and lowers the age (+2) by one; at age 0 it spawns a splash (texture 8) through
the allocator `0x80041C50` if halfword +0x18 is set, then frees the slot through `0x80041D20`. `particlesBetween`
paired a slot only while its age did not fall, a rule inferred from other owners (Glimmer's sparkles step +2 and 0),
so every rain line was unpaired and drawn as the later tick.

## Fix
A particle is new exactly when the allocator hands its slot out: `0x80041C50` (Spyro 3 `0x80056434`) stamps a0 as the
kind of the cursor slot, or of a slot it takes when the pool is full, and returns it in v0. Its override runs the guest
body and marks that slot outside replays; `EffectMotion::sample` hands the marks to the sample, and `particlesBetween`
pairs a slot while pool, texture and kind hold and it was not handed out since the earlier sample. The age rule is gone.

## Evidence
Same walk, `scratch/fade2/cut23.py r3 16 6 120 2700`: in-betweens differ from each neighbour 1.25-1.47 against a
neighbour distance of 2.2-3.2 (`between.py`), the streaks step evenly each present (`scratch/rain/r3-rain.png`,
`r3-grid.png`), `effects: 698 of 8705 item draw(s) unpaired`. Glimmer (`scratch/rain/count.py`) stays at 11.5%
unpaired against 11.6% under the age rule. fps60 0 and 1 give an identical `fielddigest` over 2267 fields
(`scratch/rain/leak0.dig`, `leak1.dig`).
