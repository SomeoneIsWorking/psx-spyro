# 0234 — Spyro 3's new-game cutscene switches between 16:9 and a 4:3 pillarbox shot by shot

Status: closed (2026-11-04, spyro23d): the pillarboxed shots are authored 4:3 pictures, kept at 4:3 by decision.
Owner: the guest's intro story overlay (resident at `0x80074xxx` during the story); no host owner decides the aspect.

## Observed
From boot at 16:9 (judgement settings, hidden, unpaced), Start then New Game from the title, the opening cutscene
("An Evil Plot Unfolds...", "A Powerful Villain Emerges...", "A Desperate Rescue Begins...") draws some shots
across the whole 16:9 canvas and others only in the 4:3 centre with dark margins. In the walk (a shot every 100
steps from title step 1600): pillarboxed at steps 2500-2800, 3300-3600, 4000-4400; full width at 3000-3200,
3800-3900, 4600. Every pillarboxed sample carries a caption; the full-width ones do not.

## Cause
The captioned shots are not rendered scenes. They are still pictures read from the disc and shown while the next
real-time shot loads:

- The story overlay's state machine (state word `0x8006C5E4`) uploads the picture with LoadImage `0x8005A478`:
  `0x80074D28` sends RECT (512, 0, 512, 256) from the buffer at `[0x800722C0]`, `0x80074D6C` the next page
  (512, 256) from that buffer + 0x40000. Nothing is projected while it is shown (no GTE calls, no polygons from
  step 2360 to the still's first frame at 2445 in the repro).
- Each frame then draws the picture as four 128x216 textured sprites (`0x64`, tpage `E1000508..0E`: 15-bit from
  VRAM x 512..1023) at (0..511, 12), modulated 0x20 for the fade-in, and the caption over it.

The picture is 512 dots wide, the retail 4:3 view; the canvas margins have no source pixels. The record path shows
exactly what the guest drew: the picture in the centre and the guest's own clear in the margins. The full-width
shots are real-time 3D through the widened projection, so the framing changes at every picture/3D cut.

## Decision
The pictures stay at their authored 4:3, centred, caption where the game draws it. Stretching them distorts the
art; zooming to fill 16:9 crops a quarter of the picture and pushes the caption (rows 182..221 of 216) off screen;
there is nothing to fill the margins with that the game drew. Same rule as an FMV: authored 4:3 frames are not
widened.

## Evidence
Repro route: `scratch/spyro23d/tools/cut.py` (Start/Cross taps every 100 steps from title step 1600).
`scratch/spyro23d/cut1/sheet.png` (steps 2500, 3100, 3400, 3900), `cut2/rec.png` (record image: the still at native
detail vs a widened 3D shot), `cut3/vram.png` (VRAM at step 2500: the picture at (512, 0), both display buffers
showing it), `edge/sheet.png` (steps 2400-2496: black hold, then the picture fading in already 4:3).
