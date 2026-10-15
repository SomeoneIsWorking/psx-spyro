# Spyro 3

`SCUS_944.67` (USA), authenticated by `executable.json`. `Spyro3Runtime` is a direct runtime: it binds
no Spyro 1 code, and its overrides are keyed by image identity plus address.

## Boot and frame loop

The shared `spyro::BootPrefixFrameDriver` (facts in `core/spyro3_boot_facts.h`) runs the retail boot
prefix `0x8002AB38` to its return, then game main's pair each step: update `0x80055400`, draw
`0x8001E638`. Every guest VSync reaches the framework's libetc boundary (`0x8005956C` never runs)
and comes back as a frame-boundary exit the driver answers with one field. libcd is the framework's
stock seam at its measured leaves (`CdRead 0x8005D96C`, `CdSync 0x8005E074`, `CdCommand 0x8005E0BC`).

## Native overrides

| address | owner | what |
|---|---|---|
| `0x8005D35C` | `psx::hle::StockLibgte` | SetGeomOffset; the one SetGeomScreen write stays guest code |
| `0x80022378` | `game/render/terrain/guest_terrain_*` | the terrain drawer, every pass; a Record producer |
| `0x8004E664` | `game/render/engine_render_overrides.h`, `game/render/terrain/guest_depth_bins.*` | the ordering-table flatten: assigns each packet its bin, then runs the original; the second-pass flattens of states 4 and 0xD (returns `0x8001EA40`, `0x8001EB4C`) assign none |
| `0x80030478` | `game/render/field/guest_moby_*` | the moby visibility walk |
| `0x80029904`, `0x80029BB0`, `0x800289C8`, `0x800291B8` | `render/spyro3_hud_anchor.*` | HUD counters, lives and their icons and values anchored at the widened edges |

## Presentation

Record path; 60 fps is psxport F-FPS60 (`docs/features.md`). The terrain drawer saves its field's
camera, sector visibility and the guest words it read, and `guest_terrain::TerrainStateProducer`
redraws the terrain from them at any t. `spyro::FrameCut` declares a cut on a new game state
(`0x8006E344`), level (`0x8006C5BC`) or a camera placement by the controller `0x800144B4`
(`0x8006E044`/`0x8006E048`). Widescreen keeps the guest's projection and draw area and widens the
canvas. Tests: `tests/test_spyro3_record.cpp`.
