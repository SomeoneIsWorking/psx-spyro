# 0251 — Guest addresses from the decomp symbol map

Workspace rule (re-harness, 2026-11-13): RE knowledge lives in the decomp repository; the port reads guest addresses
from the decomp's symbol map as generated constants, never a hand copy. Each title's decomp is our fork, a submodule:

| title | fork | symbol map |
|---|---|---|
| Spyro 1 `SCUS_942.28` | `external/open-spyro` | `config/symbol_addrs.txt` (upstream's, our names appended where it had none) |
| Spyro 2 `SCUS_944.25` | `external/spyro-2` | `preparation/symbol_addrs.txt` (the path its splat config names; upstream names live in its source, so the file lists theirs at the addresses the port reads, then ours) |
| Spyro 3 `SCUS_944.67` | `external/spyro-3` | `preparation/symbol_addrs.txt` (as Spyro 2) |

Not started: it rewrites address constants in most title headers, which collides with work in flight. Run it when
the tree is quiet, one title and subsystem per commit.

## Today

- Hand-copied addresses (`k<Name> = 0x80xxxxxxu`, a base plus offset with the address in a comment, or a designated
  initializer `.field = 0x80xxxxxxu` in a facts struct) in 85 files under `titles/spyro1`, 12 under `titles/spyro2`
  and 13 under `titles/spyro3`. The largest are `titles/spyro1/core/guest_functions.h`, `guest_globals.h`,
  `guest_gp.h`, `titles/spyro1/render/terrain_facts.h`, `titles/spyro{2,3}/render/render_facts.h`,
  `titles/spyro{2,3}/core/boot_facts.h` and `draw_section.h`.
- Spyro 1's names follow `external/spyro-1` (TheMobyCollective), while the map is open-spyro's: 398 addresses carry
  a different name there (`external/open-spyro/docs/our-names.md`, "Names that differ").
- Structure field offsets (119 in Spyro 1) and code sites inside functions (return addresses, branch joins) are
  listed in each fork's `docs/our-names.md`; overlay-range addresses (WAD images) are listed there too.

## Generator

One tool in psxport, shared with Tomba! 2 (psx-tomba plan 0065): `tools/symbol_constants.py` reads splat
`symbol_addrs` files and writes one header per image, `namespace <title>::sym { inline constexpr std::uint32_t
<name> = 0x...u; }`, plus `size` where the map gives one. A CMake function (`psxport_symbol_constants(<target>
IMAGE <name> MAP <file>...)`) generates them under `build/`; they are never committed.

- Names are emitted as the map spells them. The port adopts the map's name (open-spyro's for Spyro 1) and the
  `our-names.md` row is deleted when its constant goes; a name we want changed is changed in the fork.
- Two names at one address, or one name at two addresses in one image, fail the generation.
- Field offsets stay in the port until the fork carries the structure in its headers; code sites are written as
  `<function> + 0xNN` against a map symbol.
- WAD overlay addresses need the overlay's own map (each fork splits its overlays separately); until then they stay
  hand-written and listed in `our-names.md`.

## Order

1. Write the tool and CMake function in psxport; build `spyro_port` with headers nothing includes yet.
2. Spyro 2, then Spyro 3: `render_facts.h`, `boot_facts.h`, `draw_section.h`, `logo_facts.h`, then the rest of
   `titles/spyro{2,3}`; one commit per header. Each missing name goes into the fork first (fork commit, submodule bump
   in the port change that uses it).
3. Spyro 1: `guest_functions.h`, `guest_globals.h`, `guest_gp.h`, then `titles/spyro1/render`, then
   `titles/spyro1/core` by subdirectory. This is where the spyro-1 to open-spyro renames land.
4. Fields: as each fork gains a structure header for a global the port reads (Spyro 1's `g_Camera`, `g_Spyro`,
   `g_Environment` first), replace the port's offsets with the generated ones.
5. Build with Clang; one warp per title to show the bindings still fire.
