# 0216 — Spyro 1 inventory row band stops at the 4:3 edge

Status: fixed (2026-10-28). Owner: `titles/spyro1/render/inventory_band.*`.

## Observed
At 16:9 the dark band behind each inventory row (the "Home" row after `warp 10`, pause, two downs, cross) ends at
the 4:3 picture's edges while the world and the full-screen tint continue into the margins
(`scratch/player-bugs-8/inv0/inventory.png`).

## Cause
The inventory handler `0x8001A40C` writes the band inline: a semi-transparent POLY_F4, colour 0x28 grey, rows
row-10 to row+9, x `D_800757CC` to `D_800757CC + 0x1FF` (decomp of `0x8001A40C`; store watch on packet
`0x80187CB8`, block `0x8001BE6C`), linked by AddPrimToOT `0x800168DC` at `0x8001BEC0`. At rest `D_800757CC` is 0, so
the band ends at column 511, one short of the buffer edge. The record's margin rule continues a flat rectangle into
the margins only when it reaches both buffer edges (psxport `RecordPlanner::Builder::addCanvasMargins`), as the
full-screen tint's 0..0x200 quad does, so the band was left at 4:3.

## Fix
`inventory_band` is a state producer keyed at the band's AddPrimToOT call (return address `0x8001BEC8`, seen by the
native `dl_link_native`) with the shaded line's call key (site, turn, inventory page; `shaded_line::callObject`). It
saves the band's colour and corners and renders it from `D_800757CC` to the screen's right edge, 0x200 wide like the
tint, so at rest the margin rule continues it across the 16:9 canvas; between saves the corners and colour move
with `t`. The guest's packet and VRAM are unchanged.

## Shown
`scratch/player-bugs-8/fix1` (unpaced, ires 0 at 8x, fps60, sink 3840x2160, `inventory.py`: warp 10, start, down,
down, cross): the band spans the whole 3840-wide present; pixel (40,1100) in the left margin is (41,66,41) against
(49,90,41) on HEAD (`inv0`), and the last 300 presents are all `composed=1`. A page slide needs a second visited
homeworld, which `warp` does not give, so the band during a slide is not shown.
