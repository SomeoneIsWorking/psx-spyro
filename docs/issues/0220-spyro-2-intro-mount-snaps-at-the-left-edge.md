# 0220 — Spyro 2 intro: Ripto's mount snaps at its in-between leaving the left edge

Status: open. Owner: unknown (moby producer path, `game/render/`).

## Observed
Spyro 2 boot intro, 16:9, `fps60=1`, unpaced, dump from fence 900. At fence 1065 the in-between draws
the mount's tail almost at the new frame's pose (mean difference 0.23 to the new frame vs 2.14 to the
previous one). The present is composed with no cut (`[presentseq] fence=1065 ... composed=1 cut=0`).
No other in-between from 900 to 1200 is held or off halfway.

## Cause
Not traced. The difference lies only on the mount's tail polygons as they sweep past the left edge, so
the likely cause is that object's producer pairing (an identity or model change between the two
records) rather than the presenter.
