# 0216 — Spyro 1 inventory row band stops at the 4:3 edge

Status: fixed (2026-10-28). Owner: `titles/spyro1/render/inventory_band.*`.

## Observed
At 16:9 the dark band behind each inventory row (the "Home" row after `warp 10`, pause, two downs, cross) ends at
the 4:3 picture's edges while the world and the full-screen tint continue into the margins
(`scratch/player-bugs-8/inv0/inventory.png`).

## Cause
The inventory handler `0x8001A40C` writes the band inline: a semi-transparent POLY_F4, colour 0x28 grey, rows
row-10 to row+9, x `D_800757CC` to `D_800757CC + 0x1FF` (decomp of `0x8001A40C`; store watch on packet
`0x80187CB8`, block `0x8001BE6C`), and links it with `0x800168DC` at `0x8001BEC0`. `0x800168DC` takes only the packet
and appends it to the list whose header `[0x8007581C]` points at; that header is the overlay pair
(`render/terrain_facts.h` `overlayPair`), which `depth_bins::assignOverlay` maps to `kOverlaySlot`. At rest
`D_800757CC` is 0; a polygon leaves its right edge out, so the guest band covers columns 0-510 and column 511 is not
banded. The full-screen tint's quad is the guest's own 0..0x200 and reaches both buffer edges. The record continues a
flat rectangle into the margins only when it reaches both (psxport `RecordPlanner::Builder::addCanvasMargins`), so
the tint continued and the band did not.

## Fix
`inventory_band` is a state producer keyed at the band's `0x800168DC` call (return address `0x8001BEC8`, seen by the
native `dl_link_native`) with the shaded line's call key (site, turn, inventory page; `shaded_line::callObject`),
emitting into `kOverlaySlot` as the guest links it. It saves the band's colour, corners, and whether the canvas is
widened (`psx::present::widePresentation`) and renders the guest's quad, with corners and colour moving with `t`
between saves. The widescreen decision is the producer's: on a widened canvas a band whose left edge is column 0 has
its right edge on the buffer's edge (x 0x200), so the margin rule continues it across the canvas without an unbanded
column at 511 between the band and its right margin. At 4:3, and whenever the band does not start at column 0 (a
page slide), it is exactly the guest's quad. The guest's packet and VRAM are unchanged.

## Shown
`scratch/player-bugs-8/r2-aspect1` and `r2-aspect0` (unpaced, ires 0 at 8x, fps60, sink 3840x2160,
`inventory.py`: warp 10, start, down, down, cross; `ASPECT` 1 and 0), row y 1100:

| x | column | HEAD before (`inv0`) | 16:9 | 4:3 |
|---|---|---|---|---|
| 40 | left margin | (49,90,41) | (41,66,41) | pillar (0,0,0) |
| 3349 | 510 | — | (41,66,41) | (41,58,41) banded |
| 3354, 3357 | 511 | (49,90,41) | (41,66,41) | (49,90,41) not banded, as the guest draws it |
| 3800 | right margin | (41,82,41) | (41,58,41) | pillar |

At 16:9 column 511 is banded: that is the widescreen choice above, not retail. The last 300 presents of both runs are
`composed=1` with no `:error]` line. A page slide needs a second visited homeworld, which `warp` does not give, so the
band during a slide is not shown. No console-oracle frame was compared.
