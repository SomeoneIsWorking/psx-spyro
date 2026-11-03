# 0229 — Spyro 1's portal arrival offers only the portal path's first node

Status: open (2026-11-04). Owner: `titles/spyro1/core/debug/teleport.cpp:spyro1::teleport::toPortal`.

## Observed
Each Artisans and Peace Keepers portal has a two-node path (`gates`: `node=0`, `node=1`). `portal` places Spyro at
node 0, in front of the portal. The removed `gate-teleport <gate> <node>` could also place him at node 1, the far end
of the portal's path; the catalogue has no arrival for it.

## Next
Name what node 1 is from the portal moby's path data (inside the portal or behind it) and, if it is a place a player
reaches, add it as its own arrival under the level.
