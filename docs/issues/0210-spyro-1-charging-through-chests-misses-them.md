# 0210 — Spyro 1 charging through chests misses them

Status: closed (2026-10-27). Owner: `titles/spyro1/core/moby/native_moby_collision.cpp` (`testBoxShape`).

## Observed
Player report `20261023-181631-charging-through-chests-goes-through-them-instea`, pad frame 6795: Spyro charges into a
basket chest (moby class 194) and passes through it; the chest stays whole.

## Reproduction
`scratch/player-bugs/run_replay.py <report> <out> 3750 PSXPORT_NOPACE=1` with the report's `repro.pad` and a copy of its
`memcard.mcr`. Before the fix the chest (moby 80) is still in state 0 with Spyro inside it at 6795.

## Cause
`native_moby_collision.cpp:testBoxShape` read the box's y extent from record offset `yExtent - 0x10` instead of `6 - 0x10`
when the first extent was negative; the asm at `0x8004E760` loads the constant 6 in the `bltz` delay slot. Box shapes
reached through that arm never hit. The override differential
(`PSXPORT_OVERRIDE_DIFF=moby_collision_walk`, every call) showed it: call 10505, original v0 `0x8016EEB8`, native 0.

## Fix
The slot is 6 when the extent is negative, else 4, as in the asm.

## Proof
Differential over the whole replay (`scratch/player-bugs/chest/diff3`): 12616 calls, 12616 match, 0 mismatch. Replay
`scratch/player-bugs/chest/fixed`: at 6815 the chest is in state 253 with its debris and gem spawned
(`padshot_6815.png`).
