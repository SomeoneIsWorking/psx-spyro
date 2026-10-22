# 0208 — Spyro 2 `warp 10` faults in `field_redraw::saveField`

Status: open, not traced.

## Observed
Stepped REPL run, Spyro 2 picked, `warp 10` from the title screen (2026-10-22, 8x on a 3840x2160 sink, fps60):
`[warp] level 10 started from the title screen at f1420`, then `[watchdog] FAULT (signal) 11` with
`spyro::field_redraw::saveField(Core&, const guest_render_globals::Globals&)+0xd2` called from
`spyro::engine_render::flatten<spyro2::kRenderFacts>`. A build of HEAD before the 0169 place rework (psxport issue
0169) faults the same way, so the rework did not cause it.

## Next
Run the reproduction under the debugger and read which guest pointer `saveField` dereferences at +0xd2 on the first
frame of the new level.
