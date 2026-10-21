# 0206: Spyro 1 title screen in-betweens are shown whole

Status: fixed

## Observed
Spyro 1, fps60, 16:9, real speed, unmapped window, boot with no input, `PSXPORT_DEBUG=compose`: from
the title screen's first frame after the logo intro (logic about 586), every in-between logs
`in-between shown whole: entry N unkeyed, packet 0x80187bb4` (or `0x801a3bb4`, the other packet
buffer), 211 in one boot-to-title run. Each in-between there repeats its real frame, so the
incoming logo and the PRESS START screen move at 30 fps.

## Cause
The title overlay's logo sprite drawer (spyro-1 decomp `src/overlays/titlescreen.c`
`TitlescreenDrawSprite`, called under GamestateDraw `0x8001ED5C`) writes a POLY_FT4 per frame that no
producer keys, so `psxport present/frame_composer.cpp:composeFrame` refuses the in-between.

## Fix
`titles/spyro1/render/title_sprites.*`: `TitleSprite` is the state producer for TitlescreenDrawSprite
0x8007CD38 (disassembled from the running title overlay: a0 x, a1 y, a2 sprite, a3 colour; one POLY_FT4 at
the cursor 0x800757B0 linked through AddPrimToOT 0x800168DC). Its override keys the packet by call site and
the sprite's column, saves x, y and the packet, and runs the body; the render moves a sprite whose
texture words match by the blended x/y and draws anything else as it is now. The drawer lives in the
title overlay (id 48 at 0x8007AA38, 14336 bytes from WAD offset 0x5B800), whose range level overlays reuse,
so `titleOverlayLanded`, called from the CD queue's transfer, installs the override against each landing of
that image.

## Proof
Boot to the title at real speed, 40 s: 1008 of 1008 in-betweens composed, no `compose` refusal (before:
421 shown whole). New-game route: none shown whole through the title, save picker and cutscene. Stepped
capture of the logo sliding in (`scratch/judder-fix/logoseq/s1018`): the logo's lowest edge moves
4..7 rows on every present.
