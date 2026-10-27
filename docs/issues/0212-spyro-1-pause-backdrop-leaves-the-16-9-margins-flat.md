# 0212 — Spyro 1 pause backdrop leaves the 16:9 margins flat

Status: open (2026-10-27). Owner: the pause draw `0x8001A40C` (guest, no native owner).

## Observed
Player report `20261023-181349-menu-judder-and-wide-margins`, pad frame 1501: in the pause menu the area outside the
retail 512-wide picture is flat green, while gameplay fills it. The same report notes judder on the menu.

## Cause
On the first pause frame the retail code renders the world once, `StoreImage`s it as four 128-wide strips, converts
them to 8-bit grey (`func_80017E98`) at VRAM (512,0) with the palette at (512,224), and from then on draws four FT4
quads over x 0..512, y 8..231 tinted RGB(0x4C,0x80,0x40) (decomp `draw.c`, pause backdrop). The widened world in the
margins is only ever rendered by the host, never stored in VRAM, so the capture has nothing for the margins and the
tinted quads cannot cover them.

## Proper fix
A native owner of the pause backdrop that captures the presented (widened) frame on the pause entry, converts it the
way `func_80017E98` does, and draws the tinted quads across `presentedBounds`. That is a new producer over the pause
draw, not a one-line widening, and was not done in the player-bug pass.

The judder half: the four border lines come from `func_8001844C`, which has no native producer (issue 0144). It was not
reproduced or assessed in this pass and stays with the redesign's replay stage.
