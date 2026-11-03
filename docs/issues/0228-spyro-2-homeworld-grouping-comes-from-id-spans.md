# 0228 — Spyro 2's warp groups levels under homeworlds by id span, not by guest data

Status: open (2026-11-04). Owner: `titles/spyro2/core/warp.cpp:spyro2::Warp::worlds`.

## Observed
The 29 level ids `kLevels` come from the level id table `0x80064940` and every name from the name table `0x800649B4`,
but the homeworld a level sits under is derived: ids from one homeworld (10, 30, 50) up to the next go under it
(`kHomeworldSpan`). The grouping matches the game for the shipped ids, but no guest table states it.

## Next
Find the table the game uses for a homeworld's portals or its Atlas page (the guidebook lists each homeworld's
levels) and group from it.
