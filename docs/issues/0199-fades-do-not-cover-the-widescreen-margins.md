# 0199: fades do not cover the widescreen margins

Status: open

## Observed
USER, 2026-10-20: "fade effects don't cover widescreen". At 16:9 a fade darkens the 4:3 centre, and
the margins either side stay at full brightness.

## Next
Find the guest's fade primitive (a full-screen translucent or subtractive tile at the display's 4:3
width) and its owner in the widescreen path. A primitive whose draw area spans the whole display buffer
has to span the canvas, the same rule `planRecord` applies to fills that span a buffer's columns
(psxport `record_raster_setup.h`). Decide whether the guest site needs a title-owned widening or the
record rule is incomplete. Prove it with a 16:9 screenshot taken mid-fade.
