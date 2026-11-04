# 0231 — Spyro 2 and 3's intro logo card leaves the 16:9 margins open

Status: fixed (2026-11-04, spyro23c).
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
the logo is not stretched; nothing is drawn at 4:3. A side's quad between two rows is drawn only while both rows'
edge vertices reach that edge of the 4:3 view, so the margin shows the world exactly when the 4:3 picture shows it at
that edge (Spyro 3's card wobbles past the edges from f506, Spyro 2's swings off the right edge from f362); a card
narrower than the view that still reaches an edge gets quads long enough to reach the canvas edge. The quads go in
the bin before the entry's depth bin (entry `+0x18`): SCUS_944.25 `800453A4`..`800453D4` links the drawer's local
depth buckets into the ordering table from that bin less its unused far buckets, nearer buckets into lower bins. A bin from the corners' own depth let the ground under the card sort in
front of the bottom rows (Spyro 2 stepped corners, Spyro 3 short wings).

The extension projects each vertex as the drawer loads it: `VXY0 = x + (y << 16)` (SCUS_944.25 `800447F8`,
`80044B8C`), so a negative model x borrows one from y. Packing the halves separately put some row vertices one pixel
off the card's own corners, the Spyro 2 hairline along the left seam from f380.

Shown (16:9, fps60=1, eight presents each, in-betweens included): `scratch/spyro23c/f2w/sheet.png` (Spyro 2
f350, f380, f420, f560), `f2w/f420left.png` and `f380left.png` (left seam, no hairline), `scratch/spyro23c/f3w/sheet.png`
(Spyro 3 f506, f512, f524); 4:3 (retail picture): `p2n/`, `p3n/all.png`.

## Remaining
- While only some rows of a card edge reach the 4:3 edge, the margin beside it is part black, part world, stepped at
  the rows (Spyro 2 f560, Spyro 3 f512 in-betweens); the 4:3 picture shows a sliver of world there at the same time.
