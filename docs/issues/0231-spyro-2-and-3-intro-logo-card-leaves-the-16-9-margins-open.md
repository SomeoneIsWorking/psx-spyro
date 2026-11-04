# 0231 — Spyro 2 and 3's intro logo card leaves the 16:9 margins open

Status: margins filled (2026-11-04, spyro23c); open for the Spyro 2 seam hairline below.
Owner: `game/render/field/intro_card.cpp` (wraps the moby drawer).

## Observed
From boot at 16:9 (judgement settings, hidden, unpaced), the first screen after the disc logos, the "Developed by
Insomniac" card (Spyro 2 f330..f500, Spyro 3 "Created and Developed by" from f500) showed the title level in both
margins while the card was face-on. At 4:3 the card fills the screen until it turns.

## Cause
The card is a moby of the title level, drawn by the moby drawer (SCUS_944.25 `0x80044504`, SCUS_944.67
`0x80031124`): Spyro 2 record 3 (mesh 3, a hidden-record poke at f360 removes it: the earlier "record 3 is the
flare" note was wrong), Spyro 3 record 0 (mesh 0). Its model just covers the 4:3 view; the widened canvas keeps
retail's projection, so the margins show the world past its edges.

## Fix
`intro_card::draw` calls the drawer, then, on a widened canvas in the title game state, finds the card's render-list
entry by mesh, decodes its pose (`moby_pose::entryPose`, the drawer's vertex streams and frame-B blend), and
continues each front-face row (`titles/<t>/render/intro_card_facts.h`) outward by the share of its width the canvas
adds, projected through the entry's rotation and translation as black F4 quads. They turn and recede with the card;
the logo is not stretched; nothing is drawn at 4:3. The quads go in the bin before the entry's depth bin
(entry `+0x18`, where the drawer starts linking the card's polygons); a bin from the corners' own depth let the
ground under the card sort in front of the bottom rows (Spyro 2 stepped corners, Spyro 3 short wings).

Shown: `scratch/spyro23c/p2w/all.png` (Spyro 2 f330..f500), `p3w/all.png` (Spyro 3 f500..f650) at 16:9 fps60=1,
`p2n/`, `p3n/all.png` at 4:3 (retail picture).

## Remaining
- Spyro 2, from f380 as the card turns: a one-pixel hairline of the world shows along the seam between the card's
  left edge and its extension's lower rows (`scratch/spyro23c/p2w/f420left.png`). The card's silhouette there is
  not the straight line between the row vertices; the row table likely misses an edge vertex between rows 19 and 15.
