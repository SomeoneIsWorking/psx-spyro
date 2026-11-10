# 0237 — Spyro 2 and 3: what the draw body's replay does not move yet

Status: open. Owner: `game/core/draw_section.*` (the replayed states), `game/core/draw_motion.*` (the tracks),
`titles/spyro2|3/core/draw_section.*` (each image's switch); plan `docs/plans/spyro23-replay.md`.

## Observed
Since field_redraw and the S2/S3 producers were deleted (2026-11-10), every Spyro 2/3 in-between is a replay of the
draw body 0x800156FC / 0x8001E638. Gameplay moves halfway (Glimmer `warp 0/1/0`, Sunrise Spring `warp 0/0/0`,
`scratch/replay23/s2c`, `s3c`). These do not:

1. **Loading screen, state 5** (S2 0x800151F0, S3 0x80059038): never replayed, held at 30 Hz. S2's handler polls
   until it runs out of budget at 0x800577A0; S3's waits in VSync at 0x8005927C (`scratch/replay23/s3a.log`, 216
   refusals before `replaysState` named the state). A state the switch has no case for (S3 8, 0x11) is not
   replayed either.
2. **S3 level load, state 3** (0x8001DC5C): one refusal per warp, budget exhausted at 0x8005C224, during the load's
   fade (`s3c.log`). Not traced.
3. **Untracked inputs step at 30 Hz**: the dispatcher's fades (S2 0x80066F68 / 0x80067150; S3 0x8006C598 /
   0x8006C7C0, red flash 0x8006C6D4), the letterbox height (draw-written, whole rows), Spyro 2's conversation box
   slide (its producers were deleted; the timer's address is not traced), HUD slides, sky objects, the held object
   at 0x8006AA1C / 0x8007179C, Spyro's tint, water floor, reflection shift and matrix. Plan Open 1, 5, 6, 7, 8, 10.
4. **Not shown since the change**: the title screen (state 11), cutscenes (6, 0x12), conversations (1), the pause
   menu and S3's Sparx second pass (state 4, `DrawSection::flattenFrame`), the boss and scene states. The switch
   replays them; none has been judged in frames.
5. **Pacing**: the paced Glimmer run (`scratch/replay23/pace2.log`) was taken at host load 20+ (other agents'
   Godot): every present alternates t=0.5 composed / t=1, but the simulated display shows 292 of 946 vblanks at 0.0
   from host stalls. Needs a quiet host; no paced Spyro 3 run.

## Next
Trace 2 with `decomp_pipeline.py --function-at 0x8005C224` (SCUS_944.67); add the fade and letterbox words as
tracks once their neighbours' bytes are known; show item 4 state by state through each title's warp or title route.
