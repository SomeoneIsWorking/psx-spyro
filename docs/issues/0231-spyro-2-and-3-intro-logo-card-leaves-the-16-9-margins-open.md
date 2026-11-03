# 0231 — Spyro 2 and 3's intro logo card leaves the 16:9 margins open

Status: open (2026-11-04). Owner: `game/render/field/guest_moby_*` (the moby draw of the title image's logo card).

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

## Next
Extend the card's plane, not its texture: in the card's model space, two black quads continuing its outer tile
columns past the canvas edge, projected through the moby's own composed matrix, so they turn with the card and the
logo is not stretched. Needs the card's moby class in each title image and its model's X extent; drawn only when
the canvas is wider than the buffer. Scaling the model by 684/512 would cover the margins but stretch the logo.
