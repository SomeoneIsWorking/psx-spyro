# 0212 — Spyro 1 pause backdrop leaves the 16:9 margins flat

Status: fixed (2026-10-28) for the backdrop; the border-line judder half stays with issue 0144. Owner: `titles/spyro1/render/pause_backdrop.*` over psxport's read-back texture (`docs/presentation.md`).

## Observed
Player report `20261023-181349-menu-judder-and-wide-margins`, pad frame 1501: in the pause menu the area outside the
retail 512-wide picture is flat green, while gameplay fills it. The same report notes judder on the menu.

## Cause
On the first pause frame the retail code renders the world once, `StoreImage`s it as four 128-wide strips, converts
them to 8-bit grey (`func_80017E98`) at VRAM (512,0) with the palette at (512,224), and from then on draws four FT4
quads over x 0..512, y 8..231 tinted RGB(0x4C,0x80,0x40) (decomp `draw.c`, pause backdrop). The widened world in the
margins is only ever rendered by the host, never stored in VRAM, so the capture has nothing for the margins and the
tinted quads cannot cover them.

## Fix
psxport gained a read-back texture (`present::ReadBackTexture`, `GpuDevice::declareReadBack`, `gpu::RecordReadBack`,
`record.frag` `readBackTexel`). The override of `func_80017E98` (`pause_backdrop.cpp`) declares, before each strip's
conversion, that VRAM (512,0) 256x224 at 8 bits is the index image of the displayed rows the guest just read (row 8 or
248 by `g_CurDB` 0x80075888 against `g_DB` 0x80076EE0), with the conversion's weights (4r + 3g + b) >> 3. The record
captures the canvas there, margins included, at scale; from the next record on the guest's own four FT4s sample that
capture through the grey CLUT, and the outer two continue into the margins. The unpause restore into (512,0) ends it.
The black LINE_F2 at row 231 now spans the margins as well (a flat line reaching both buffer edges counts as a
rectangle). No guest-visible change: the conversion and its uploads still run, and 4:3 has no canvas, so nothing is
captured there.

Shown (report `20261023-181349`, pad frame 1501, unpaced, ires 0 at 8x, fps60, sink 3840x2160): the margins show the
tinted scene continuing the middle at full detail, row 231 black across; every present of fences 790-805, real and
in-between, carries it (`scratch/player-bugs-5/runs/fix5`, `fix5dump`). The pause in-betweens are held (S as drawn),
not composed: the menu's unowned packets move (issue 0144), so the verdict holds them; the margins are filled either
way because the read-back lives in the record path, not in a producer. Unpausing through the control channel
(`tap start`) shows gameplay textured normally (`runs/unpause`).

The judder half: the four border lines come from `func_8001844C`, which has no native producer (issue 0144). It was not
reproduced or assessed in this pass and stays with the redesign's replay stage.
