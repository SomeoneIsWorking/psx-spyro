---
id: 188
title: Spyro 2 and 3 sector sphere cull uses the retail slope at 16:9
status: open
symptom: terrain sectors in the 16:9 margins can be culled by the 4:3 sphere test
tags: spyro2,spyro3,widescreen,terrain
created: 2026-10-15
updated: 2026-10-15
---

## Gap
`game/render/terrain/guest_terrain_classify.cpp` tests sector spheres against
`kRetailSlope`. Spyro 1's sector pass (`titles/spyro1/render/spyro1_terrain_sectors.cpp`) draws on the
widened sphere and keeps the sector marks and `animateSector` on retail's; Spyro 2/3 need the same split.
