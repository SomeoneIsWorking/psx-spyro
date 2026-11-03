# 0227 — Spyro 2 and 3 warps have only level starts, applied from the title screen

Status: open (2026-11-04). Owner: `titles/spyro2/core/warp.*:spyro2::Warp`, `titles/spyro3/core/warp.*:spyro3::Warp`.

## Observed
`warp ?` gives each level one arrival, `start`. Armed during play it replies `ok: warp armed for level N` and waits
until the player returns to the title screen (`levelChangeReady` is false), then starts the level through the title's
own start states. Spyro 1, by contrast, also enters from play and has `portal` and `dragon N` arrivals.

## What the games have
- Spyro 3: the title's state 20 calls WarpToLevel `0x800584BC(0, level id)` (`docs/re-frontier.md`); whether a call
  from play at a frame boundary is the game's own level change (as the portal path takes it) is not traced.
- Spyro 2: the title's start state calls the load starter `0x80019230(start index)`; the portal's path into a level is
  not traced.
- Neither title's portal mobys (the hub portal in front of each level) nor its checkpoints are traced, so no `portal`
  or checkpoint arrival exists yet.

## Next
Trace each hub portal's level change (the code the portal's touch runs) and give `changeLevel` that path; then list
the hub's portals as `portal` arrivals from the portal records, as `spyro1::teleport::toPortal` does.
