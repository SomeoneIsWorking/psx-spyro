# More vegetation (G006)

Denser grass, plants and flowers in Spyro 1, 2 and 3, grown where each level already grows them, drawn
by the host and never written into guest state.

## What the levels hold

Measured 2026-11-12 on RAM dumps of Spyro 2 and 3 at `warp 0/0/0`, `0/1/0` and `1/1/0`
(`scratch/veg/census.py`). It walks the near-sector table the S2/S3 terrain drawer walks (Spyro 2 table
`0x800673D4`, count `0x800673D8`, textures `0x800673F4`; Spyro 3 `0x8006D048`, `0x8006D04C`, `0x8006D068`) with
the layout of `game/render/terrain/guest_terrain_mesh.h` and `guest_terrain_polygon.h`:

- Ground vegetation is ordinary terrain: grass is a texture record on flat or sloped near faces. There
  are almost no plant cards. Two-sided faces are rare: Spyro 2 Summer Forest's texture 87 (173 faces,
  flat, on a semi-transparent page, most likely water) and Spyro 3's textures 32 (198 faces, flat), 97, 51
  and 52.
- None of Spyro 1's special-surface kinds (`external/spyro-1` `special_surfaces.c`: 0 normal, 1, 2 spiders,
  4 supercharge, 5 invisible wall, 6 portal, 7 electric) marks vegetation.
- Collision faces carry what placement must avoid (Spyro-Scope's format, GPL-3.0, read as evidence only:
  `scratch/s1load/TerrainCollision.bf.txt`, `TerrainRegion.bf.txt`): per-triangle flag byte, low 6 bits
  the special-surface index (0x3F none), high 2 bits the footstep sound (Spyro 2 `0x80034E0C` plays
  `table[0x80066F54 * 4 + (byte >> 6)]`); water is a triangle's z & 0x4000; walkable slope is below 0x17
  (Spyro 2 `0x80035E44`).

Not yet measured: Spyro 1's census (its decomp's `environment.h` structs replace the S2/S3 table walk),
and the scenery mobys that are plants (trees, bushes and flowers drawn as mobys in some levels).

## Design

1. **Which faces grow.** A title-owned, per-level data table names the ground texture records that are
   vegetation (grass, flower beds, leaf litter). A maintainer listing tool produces the candidate list
   from a level's texture records, each with a texture crop and its face count, and a reviewer marks the
   vegetation ones; the table is checked in. No runtime heuristic decides it.
2. **Where it may not grow.** At level load the host reads the collision faces under each grown face and
   drops it when it is water, carries a special surface, is steeper than the walkable slope, or lies on a
   path texture the table marks. Gameplay moby spawn points and their collision radius are cut out the
   same way.
3. **What grows.** Blades, tufts and flower heads built from the level's own textures: a tuft is a few
   crossed quads sampling the grown face's own texture record (its top rows read as blades against the
   ground's alpha), and flowers reuse the level's flower moby textures where the level has them. Placement
   is a hash of the face's guest address and a per-face index, so it is identical every run and every
   frame.
4. **How it draws.** Host-native, after the guest's terrain in the same ordering-table buckets as the
   face it stands on, through the existing terrain owner (`game/render/terrain/`), with the blended camera
   of the 60 fps in-betweens so it interpolates like the ground. Margin sectors drawn at widened aspects
   (S026, S027) grow it too. Density and draw distance come from the config owner.
5. **Switch.** A saved setting, off by default, forced off in the faithful mode and under oracle
   comparison. It writes no guest RAM, VRAM or GPU state the guest reads, and adds no collision.

## Steps

1. Spyro 1 census and the plant-moby inventory for all three titles (decomp names for Spyro 1, moby type
   ids and model counts for Spyro 2 and 3). **Partial:** the near-face census of all three step-2 levels is
   `tools/vegetation_listing.py`'s report (Spyro 1 Stone Hill: 40 textures, 6,659 near faces, lawn record
   40 alone 3,160; Spyro 2 Summer Forest: 87 textures, 8,160 faces; Spyro 3 Sunny Villa: 89 textures,
   5,791 faces). Spyro 1 reads `g_Environment` (`0x800785A8`: sectors +0, count +4, 16-byte detail
   texture records +0x18), names its texture by the polygon's links byte (+8) and packs height as
   `(word << 2) & 0xFFC`. The plant-moby inventory is not done.
2. The listing tool and the reviewed vegetation tables for one level per title (Spyro 1 Stone Hill,
   Spyro 2 Summer Forest, Spyro 3 Sunny Villa). **Done:** `tools/vegetation_listing.py <title> <ram>
   <vram> <out>` (inputs `dumpram`/`vramraw` at the level's warp) lists each record with its face count,
   flatness, two-sided and translucent faces, and cuts its full-bright 32x32 tile from VRAM into
   `tex_<n>.png` and `sheet.png`; the reviewed tables are `titles/<t>/render/vegetation_table.h`, each
   record with the crop judged.
3. Exclusion from collision and mobys, shown as a debug overlay on those levels. **Partial:** placement
   drops a grown face that shares a corner with a path record of the table, or lies in a translucent
   sector, and logs the counts per level (`[vegetation] level N: ... dropped ... beside a path`). Not
   done: the collision lookup (water, special surface, walkable slope), moby spawn points, and the
   overlay.
4. Tuft geometry and the draw in the terrain buckets, judged unpaced at `ires=0`, `fps60=1`, 3840x2160,
   then the cost in `perf-units` (GPU work per present, CPU instructions per frame) against the run spread.
   **Drawn, cost too high:** a tuft is four GT3 blades cut from its face's own texture tile and shaded
   from its face's near colour, placed by a hash of the polygon record's address and index,
   projected host-side with the draw's camera (the replay's blended one in an in-between) and inserted
   after the last terrain packet of its depth bin through `psx::gpu::HostPackets::insertAfter` at the
   flatten, depth-tested as terrain. Guest RAM is byte-identical with the setting on and off at a fixed
   step on all three levels. Cost (2026-11-14, Summer Forest step 1400 and Stone Hill step 3000, two runs
   each on Summer Forest, run spread 0): Summer Forest GPU draws/present p50 543 -> 1,967, triangles
   31.2k -> 46.1k, upload 984 -> 1,712 KiB; CPU present 142.6 -> 232.8 Minsn/frame. Stone Hill draws 823
   -> 4,735, CPU present 146.8 -> 343.9 Minsn/frame. Each blade costs about one record draw; the next
   step is to find what splits the record batches per blade (`record_rasterizer` batch rule) or build
   tufts that batch, before more levels.
5. Tables for the remaining levels, then the setting in the menu.

The setting is `more_vegetation` (saved, off by default, `enh_int`, so off in a comparison run);
`vegetation_density` (tufts per million square vertex units) and `vegetation_distance` (vertex units)
are its config knobs. Blade size is the image's `VegetationFacts::bladeHeight`.

## Proof

On each step-2 level, reached through its warp: frames with the setting on and off showing tufts only on
the marked textures, none on paths, water or at gameplay objects; halfway motion in the in-betweens;
logic state identical with the setting on and off at a fixed frame.

## State (2026-11-14)

Step 3 (collision and moby exclusion) has a first unbuilt version shelved in the stash "G006 vegetation exclusion WIP". The one-draw-per-blade cost (Stone Hill 823 to 4,735 draws per present) is not yet traced.
