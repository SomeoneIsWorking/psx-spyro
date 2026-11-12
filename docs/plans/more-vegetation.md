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
   ids and model counts for Spyro 2 and 3).
2. The listing tool and the reviewed vegetation tables for one level per title (Spyro 1 Stone Hill,
   Spyro 2 Summer Forest, Spyro 3 Sunny Villa).
3. Exclusion from collision and mobys, shown as a debug overlay on those levels.
4. Tuft geometry and the draw in the terrain buckets, judged unpaced at `ires=0`, `fps60=1`, 3840x2160,
   then the cost in `perf-units` (GPU work per present, CPU instructions per frame) against the run spread.
5. Tables for the remaining levels, then the setting in the menu.

## Proof

On each step-2 level, reached through its warp: frames with the setting on and off showing tufts only on
the marked textures, none on paths, water or at gameplay objects; halfway motion in the in-betweens;
logic state identical with the setting on and off at a fixed frame.
