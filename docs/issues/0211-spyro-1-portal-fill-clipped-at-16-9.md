# 0211 — Spyro 1 portal fill clipped at 16:9

Status: closed (2026-10-27). Owner: `titles/spyro1/render/portals.cpp` (`Portals`).

## Observed
Player report `20261023-181428-portal-effect-gets-clipped`, pad frame 2463, mark x 0-10% y 22-40%: when the camera is
inside a level portal's sky, the left margin shows the hub's terrain instead of the portal's sky.

## Reproduction
`scratch/player-bugs/run_replay.py <report> <out> 1500 PSXPORT_NOPACE=1 PSXPORT_PAD_SHOT_AT=2463`;
`scratch/player-bugs/portal/before_after_left.png` is the left third before (left) and after (right).

## Cause
`Portals` tested, clamped and filled against retail's 512-wide screen: the fill triangles `D_8006CB8C` end at x = 0 and
512, and `publishWindow`, `coverage`, `appendEdge` and the on-screen tests used the same edges, so the margins were never
covered.

## Fix
`Portals` takes the presented `ScreenBounds` (`spyro::presentedBounds`) and uses its left and right edges; the fill
triangles' corners are moved onto them (`Portals::spanned`).

## Proof
Same replay after the fix: the portal sky fills the left margin at 2463 (`portal/fixed`).
