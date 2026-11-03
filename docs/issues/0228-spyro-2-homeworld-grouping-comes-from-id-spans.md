# 0228 — Spyro 2's warp groups levels under homeworlds by id span, not by guest data

Status: closed (2026-11-04). Owner: `titles/spyro2/core/warp.cpp:spyro2::Warp::worlds`.

## Observed
The homeworld a level sat under was derived from id spans (`kHomeworldSpan`); Spyro 3's from fixed 9/9/9/10
counts, which listed Super Bonus Round as id 49 (its id is 50).

## Fix
Both read the homeworld per level index from the guest (Spyro 2 `0x80064B84`, the table the portal exit reads;
Spyro 3 levelIndexToHomeworldLevelId `0x800671A0`), and the ids from the id-to-index tables (`0x80064940`,
`0x80066FCC`); `spyro::groupByHomeworld` builds the catalogue. `warp ?` in `scratch/spyro23/p2a`, `p3a`.
