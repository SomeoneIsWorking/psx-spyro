# 0208 — Spyro 2 `warp 10` faults in `field_redraw::saveField`

Status: fixed (2026-10-27). Owner: `game/render/field/field_redraw.cpp`.

## Observed
Stepped REPL run, Spyro 2 picked, `warp 10` from the title screen (2026-10-22, 8x on a 3840x2160 sink, fps60):
`[warp] level 10 started from the title screen at f1420`, then `[watchdog] FAULT (signal) 11` with
`spyro::field_redraw::saveField(Core&, const guest_render_globals::Globals&)+0xd2` called from
`spyro::engine_render::flatten<spyro2::kRenderFacts>`. A build of HEAD before the 0169 place rework (psxport issue
0169) faults the same way, so the rework did not cause it.

## Cause
`field_redraw.cpp:placeSky` walked the waiting redraws by the header bytes in `history.state`, which are the last
save's until `saveRedraws` writes them; on the first frame of a level they named more fields than the history held,
and the walk read past the buffer.

## Fix
`placeSky` walks `history.redrawn` and `history.fields`. Spyro 2 level 21 now runs 25 s unpaced with every interval
composed (2026-10-27).
