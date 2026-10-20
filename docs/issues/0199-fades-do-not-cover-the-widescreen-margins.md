# 0199: fades do not cover the widescreen margins

Status: fixed

## Observed
USER, 2026-10-20: "fade effects don't cover widescreen". At 16:9 a fade darkens the 4:3 centre, and
the margins either side stay at full brightness.

## Reproduction
`warp 10` in Spyro 1 at 16:9 (`tools/shipping_settings.ini`), hidden window, `preseq 900` from the
warp start. The level's fade-in is presents 700-730: centre luminance 2.8 -> 74.6 while both margins
are already 123/117 (`scratch/b1/s1_fade/p0720.png`: a dark 512-column band between lit margins).

## Cause
The fade is one semi-transparent flat untextured quad covering exactly the 512 buffer columns
(`scene`: `POLY semi flat rgb=(240,240,240)` then `(64,64,64)` on fade frames). psxport
`gpu/record_raster_setup.cpp:RecordPlanner::Builder::planeClips` widens the canvas clip for it, since its
draw area spans the buffer, but the quad's own vertices stop at the buffer edge, so the canvas margins
got nothing. Only fills had the rule that spanning the buffer's columns means spanning the canvas's.

## Fix
psxport `RecordPlanner::Builder::addCanvasMargins`: a flat untextured rectangle (a sprite, or a quad
with axis-aligned edges, `gpu::flatRectangle`) whose columns are exactly the buffer's also draws the
margin columns on its rows, as one flat sprite op in the same colour and blend. Geometry that reaches
past the buffer edge (the widened 3D view) is not touched.

## Proof
Same route after the fix (`scratch/b1/s1_fade2/`): presents 659-688 brighten together, e.g. p0670
left 13.5 / centre 9.6 / right 13.7, p0680 62.8 / 48.2 / 53.3; `p0680.png` is evenly darkened across
all 684 columns with no seam. Spyro 2 (`scratch/b1/spyro2_wide/`, p0026-p0044) and Spyro 3
(`scratch/b1/spyro3_wide/`, p0609-p0627) fades darken the margins with the centre.
