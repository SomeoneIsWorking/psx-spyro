# Spyro 3

`SCUS_944.67` (USA), authenticated by `executable.json`. `spyro3::Runtime` is a direct runtime: it binds
no Spyro 1 code, and its overrides are keyed by image identity plus address.

## Boot and frame loop

The shared `spyro::BootPrefixFrameDriver` (facts in `core/boot_facts.h`) runs the retail boot
prefix `0x8002AB38` to its return, then game main's pair each step: update `0x80055400`, draw
`0x8001E638`. Fields follow the guest clock (`spyro::FieldOwner`'s host turn); a guest `VSync(0)`
reaches the framework's libetc boundary (`0x8005956C` never runs) and takes the next field. The
draw's tail waits until two fields have passed since its stamp `0x8006C5D4` (one more first while
`0x8006C678` is set; state 9 waits once), so a frame covers max(2, k + flag) fields. libcd is the framework's
stock seam at its measured leaves (`CdRead 0x8005D96C`, `CdSync 0x8005E074`, `CdCommand 0x8005E0BC`).

## Native overrides

| address | owner | what |
|---|---|---|
| `0x8005D35C` | `psx::hle::StockLibgte` | SetGeomOffset; the one SetGeomScreen write stays guest code |
| `0x80022378` | `game/render/terrain/guest_terrain_*` | the terrain drawer, every pass |
| `0x8004E758` | `game/render/overlay_pair.*`, `titles/spyro3/render/render_facts.h` | the overlay pair's link, native: names each packet crop UI but the letterbox's and the fades' |
| `0x8002E748`, `0x8002E970` | `game/render/overlay_pair.*` | the text layouts, sprites and quads: run, and the glyph chain each links inline named crop UI |
| `0x8004E664` | `game/render/engine_render_overrides.h`, `game/render/terrain/guest_depth_bins.*` | the ordering-table flatten: assigns each packet its bin, then runs the original; the second-pass flattens of states 4 and 0xD (returns `0x8001EA40`, `0x8001EB4C`) assign none |
| `0x80030478` | `game/render/field/guest_moby_*` | the moby visibility walk |
| `0x80029904`, `0x80029BB0`, `0x800289C8`, `0x800291B8` | `render/hud_anchor.*` | HUD counters, lives and their icons and values anchored at the widened edges |
| `0x80029708`, `0x800291B8`, `0x800293C4` | `render/hud_slide.*` | the widgets' slide step and digit drawers, native; in-betweens place each icon and digit between the two ticks' slides |

## Presentation

Record path; 60 fps is psxport F-FPS60 (`docs/features.md`). Each in-between is a replay of the
draw body 0x8001E638 with its inputs blended (`titles/spyro3/core/draw_section.*`,
`docs/plans/spyro23-replay.md`). `spyro::FrameCut` declares a cut on a new game state
(`0x8006E344`), level (`0x8006C5BC`) or a camera placement by the controller `0x800144B4`
(`0x8006E044`/`0x8006E048`). Widescreen keeps the guest's projection and draw area and widens the
canvas.
