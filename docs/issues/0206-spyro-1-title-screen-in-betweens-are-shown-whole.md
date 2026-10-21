# 0206: Spyro 1 title screen in-betweens are shown whole

Status: open

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
Not done. Needs a producer scoped to the title overlay's image for that drawer, drawing the sprite at
the in-between's state.
