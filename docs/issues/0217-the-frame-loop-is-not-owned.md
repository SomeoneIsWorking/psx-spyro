# 0217 — The frame loop is not owned

Status: open (2026-10-28). Owner: `titles/spyro1/core/frame/frame_driver.cpp, game/core/boot_prefix_frame_driver.cpp, game/core/field_owner.cpp`.

## What the code does
Spyro 1 runs update `0x8003385C` and draw `0x8001ED5C` (game main `0x80012204`) and `FieldOwner` delivers a field each time the guest clock crosses one. Spyro 2 (update `0x8001B140`, draw `0x800156FC`, main `0x80011ADC`) and Spyro 3 (update `0x80055400`, draw `0x8001E638`, main `0x8001200C`) run their pair each step and answer every guest VSync inside it with one field.

## Why it matters
The workspace rule (psxport `docs/workspace/WORKSPACE.md`, "Each title owns its frame loop") wants the frame driver to run the guest's per-frame update and draw and decide the cadence, replacing the guest's VSync waits. Until then the loop is on the fallback; it is not yet on `Timing::vsyncFields` either, since every guest VSync is answered with one field whatever its argument.

## What blocks it
The update/draw pairs are already driver-run; the guest VSync waits inside the draws (and Spyro 1's draw-wait flag `0x80075784`) still set the cadence. Next: replace the draws' VSync calls with the driver's field count per title.
The converted driver counts `Timing::vsyncFields` of the loop's own n at the replaced wait (psxport `docs/presentation.md`, Frame fence and pacing), so an overrunning frame keeps its VBlanks, audio and fields on the guest clock's grid; it is done in that conversion, not as a separate step on the current one-field answer.
