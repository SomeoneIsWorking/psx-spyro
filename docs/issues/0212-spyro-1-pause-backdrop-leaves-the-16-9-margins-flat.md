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
draw, not a one-line widening.

It needs a renderer capability psxport does not have (2026-10-28 assessment): the Record renderer keeps one canvas per
displayed buffer and samples textures only from the VRAM image (`docs/presentation.md`: the margin is never a texture
source), so no primitive can read the widened canvas. The pieces, in order:
1. psxport: a FrameRecord entry that snapshots the displayed canvas plane (full presented width) at the point the
   guest's `StoreImage` of the frame runs, kept until the next snapshot.
2. psxport: a texture source on a record primitive that samples that snapshot through a 256-entry grey CLUT, which is
   what `func_80017E98` builds (luma into 8-bit indices, CLUT at (512,224)); the CLUT comes from VRAM as now.
3. Spyro 1: an override of `func_8001A40C`'s backdrop that emits the four tinted FT4s over `presentedBounds` with that
   source in place of the (512,0) page, and leaves the 512-wide guest path untouched at 4:3.
Not started in the player-bug pass 3; the margins are still flat green.

The judder half: the four border lines come from `func_8001844C`, which has no native producer (issue 0144). It was not
reproduced or assessed in this pass and stays with the redesign's replay stage.
