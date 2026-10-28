# 0212 — Spyro 1 pause backdrop leaves the 16:9 margins flat

Status: fixed (2026-10-28). The backdrop fills the margins, and the pause in-betweens are composed (border lines below). Owner: `titles/spyro1/render/pause_backdrop.*` over psxport's read-back texture (`docs/presentation.md`).

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

The judder half: the menu's border lines come from `func_8001844C`, a LINE_G2 whose endpoint greys follow
`D_800770C8.m_specularTime` (decomp `draw.c`), so every packet moved each frame and the owner verdict held every pause
in-between (`UnownedMoving guest fn=0x8001844C`, 299 of 299 in-betweens `composed=0` from fence 698 on HEAD). The
producer `titles/spyro1/render/shaded_line.*` owns each call under a key made of its endpoints, saves its four packet
words, and draws an in-between as the later packet with each endpoint's colour moved between the two saves.

Shown (same report, unpaced, ires 0, fps60, sink 3840x2160, `scratch/player-bugs-6/fix1`, `fix4`): no `UnownedMoving`
row; every in-between from fence 690 to the run end `composed=1` (501 of 501). Twelve consecutive presents of the
pause: the left border's grey steps 189, 181, 173, 164, 156, 148 a channel across presents in one direction, and each
in-between lies between its neighbours except edge pixels of the spinning HUD gems. Paced run (`paced2`), after the
selector's pre-roll ends (fence 901): a simulated 60.00 Hz display over 9.8 s shows 590 vblanks with 2 repeats (the
59.94 Hz phase), 0 skips, 294 in-betweens shown, none held.

## Margin rule in the other titles
The change also widened psxport's margin rule: a flat line, and a flat rectangle reaching past both buffer edges, now
fills the margins. A 40 s unpaced run from boot of every product with a log at each newly filled rectangle
(`*/scratch/player-bugs-5/paced40-margins/run.log`, probe not committed) found: C-12 per-row LINE_F2 backdrops
(x 0..512, rgb 0-99); Toy Story 2 grey fade sprites 320..832; Vagrant Story an rgb 8 sprite 512..1152 and full-VRAM
black clears; Spider-Man, Mega Man X4 and Tekken 3 one full-VRAM black clear each; Spyro 1 (attract),
Crash, Crash Bash, CTR and Tomba 2 none. All are screen-wide fills or fades. C-12's title screen and first mission
street were checked as pictures (`psx-c12/scratch/player-bugs-5/shots-probe{,-game}/strip.png`): margins continue
the centre with no banding.
