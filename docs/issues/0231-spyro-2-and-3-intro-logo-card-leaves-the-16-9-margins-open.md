# 0231 — Spyro 2 and 3's intro logo card leaves the 16:9 margins open

Status: open (2026-11-04). Owner: not yet found; a drawer under the dispatcher `0x800155A0` in the title screen draw.

## Observed
From boot at 16:9 (judgement settings, hidden, unpaced), the first screen after the disc logos, the "Developed by
Insomniac" card (Spyro 2 f330..f420, Spyro 3 "Created and Developed by" from f510), shows the title scene in both
margins while the card is face-on, and the card's edges as it swings away. At 4:3 the card fills the screen until
it turns. Evidence: `scratch/spyro23/title-card-margins.png` (Spyro 2 f360 over Spyro 3 f510),
`scratch/spyro23/s2title/strip.png` (Spyro 2 f330, f360, f400).

## Cause
The card is a moby in front of the title camera, not a screen overlay: Spyro 2 f354 draws it as eight `0x3C`
textured quads shaded `7F7F7F` (packets `0x801BE4D4`..`0x801BE6D0`, written under the object half `0x8004C534`
after the moby walk), a 4x2 grid of 96-texel tiles spanning x -7..528 of the 512 buffer. The widened view keeps
retail's projection and shows more world at the sides, so a model authored to just cover the 4:3 view covers only
the centre of the 684-column canvas. The fade rule (issue 0199) does not apply: these are textured model polygons.

## Owner search (2026-11-04, spyro23b)
- The card is not one of the title level's nine mobys (array at `[0x80066F14]` = `0x80093BCC`, meshes 0..8, all
  angles 0): moving each record's Y away for two fields at f360 leaves the card drawn; record 3 is the lens flare
  (`scratch/spyro23b/hide/all.png`, `hide3/ab.png`). So the earlier "moby in front of the camera" reading is wrong.
- The camera is static over the card (angles `0x1000,0x0008,0x0186`, position `0x1B048,0x9E60,0x6B14` at f354 and
  f410); the swing is the card's own motion.
- A store watch over its packets at f410 names the dispatcher `0x800155A0` called from the title screen draw
  `0x80015560` (mask `0xD`: object half `0x8004C534`, terrain, sky `0x80029170` and `0x8004C66C`). The packets
  keep one address from f330 to f354 and are rewritten only while the card moves.
- Not Spyro's drawer: its base `0x80069FF0` is zero throughout.

## Next
Decompile `0x8004C66C` (walks `DAT_80066FA8` entries after the sky) and the object half's `0x80046FD8`/`0x800499D4`
to find which one owns the card's object and its model; then extend that model as below.
Extend the card's plane, not its texture: in the card's model space, two black quads continuing its outer tile
columns past the canvas edge, projected through the moby's own composed matrix, so they turn with the card and the
logo is not stretched. Needs the card's moby class in each title image and its model's X extent; drawn only when
the canvas is wider than the buffer. Scaling the model by 684/512 would cover the margins but stretch the logo.
