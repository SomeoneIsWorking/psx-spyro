# 0227 — Spyro 2 and 3 warps have only level starts, applied from the title screen

Status: closed (2026-11-04). Owner: `titles/spyro2/core/warp.*:spyro2::Warp`, `titles/spyro3/core/warp.*:spyro3::Warp`.

## Observed
`warp ?` gave each level one arrival, `start`. Armed during play it waited until the player returned to the title
screen (`levelChangeReady` was false), then started the level through the title's own start states.

## Fix
- From gameplay (game state 0) each title now takes the game's own level start: Spyro 2 `0x80019230(0)` then the
  level id, as the credits cheat's return and the pause menu restart a level; Spyro 3 WarpToLevel `0x800584BC(0, id)`,
  the level-warp cheat's call.
- `portal`: enter the level, then leave it for its homeworld through the game's exit (Spyro 2 `0x80019928(-1)`, the
  pause menu's Exit Level; Spyro 3 WarpToLevel(0, homeworld)), whose loader places Spyro in front of that level's
  portal. Evidence and addresses: `docs/re-frontier.md` title.warp.

Shown in hidden runs (`scratch/spyro23/p2b`, `p3a`): gameplay to Autumn Plains and Midday Garden; Idol Springs and
Cloud Spires `portal` land in front of their portals.

## Remaining
Spyro 2's boss arenas (ids ending in 6) and Spyro 3's slot-8 levels and Super Bonus Round have no `portal`: the game
leaves them through another load and arrives elsewhere. Neither executable names its NPCs or checkpoints, so NPC
arrivals stay `moby-teleport <class> <n>`.
