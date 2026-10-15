# Spyro 2

`SCUS_944.25` (USA), authenticated by `executable.json`. `spyro2::Runtime` is a direct runtime: it binds
no Spyro 1 code. Lightrec caches and overrides are keyed by image identity plus
address, so Spyro 1's identical addresses never select this title's behaviour.

## Boot and frame loop

The shared `spyro::BootPrefixFrameDriver` (facts in `core/boot_facts.h`) runs the retail boot
prefix `0x80011E9C` to its return, then game main's pair each step: update `0x8001B140`, draw
`0x800156FC`. Every guest VSync reaches the framework's libetc boundary (`0x80058EDC` never runs)
and comes back as a frame-boundary exit the driver answers with one field; each step presents one
field. The route `tools/title_route.py --title spyro2` reaches Glimmer gameplay at field 3950.

## Native overrides

| address | owner | what |
|---|---|---|
| `0x80057AF8`, `0x80057AE8` | `psx::hle::StockLibgte` | SetGeomOffset and SetGeomScreen |
| `0x80023BB4` | `game/render/terrain/guest_terrain_*` | the terrain drawer, every pass; a Record producer |
| `0x8001B2A8` | `game/render/engine_render_overrides.h`, `game/render/terrain/guest_depth_bins.*` | the ordering-table flatten: assigns each packet its bin, then runs the original |
| `0x80043858` | `game/render/field/guest_moby_*` | the moby visibility walk |
| `0x8005251C`, `0x800520CC` | `render/hud_anchor.*` | HUD counters and sprites anchored at the widened edges |

## Presentation

Record path; 60 fps is psxport F-FPS60 (`docs/features.md`). The terrain drawer saves its field's
camera, sector visibility and the guest words it read, and `guest_terrain::TerrainStateProducer`
redraws the terrain from them at any t; `spyro::depth_bins` gives every packet its ordering-table
bin; `spyro::FrameCut` declares a cut on a new game state, level or camera placement.
Widescreen keeps the guest's projection and draw area and widens the canvas. Status and open items:
`docs/project-state.md` S026/S028.
