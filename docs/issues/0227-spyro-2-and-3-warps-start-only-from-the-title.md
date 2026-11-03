# 0227 — Spyro 2 and 3 warps have only level starts, applied from the title screen

Status: closed (2026-11-04). Owner: `titles/spyro2/core/warp.*:spyro2::Warp`, `titles/spyro3/core/warp.*:spyro3::Warp`.

## Observed
`warp ?` gave each level one arrival, `start`. Armed during play it waited until the player returned to the title
screen (`levelChangeReady` was false), then started the level through the title's own start states.

## Fix
- From gameplay (game state 0) each title now takes the game's own level change: Spyro 2 the pause menu's Exit Level
  `0x80019928(-1)` with its destination `0x8006715C` set to the level, the game's fade and load (state 7); a
  homeworld, which the game leaves a level only for, is reached through its first level and that level's exit.
  Spyro 3 WarpToLevel `0x800584BC(0, id)`, the level-warp cheat's call. The storybook load `0x80019230(0)` was
  rejected: it overwrites level data a pending terrain in-between still reads (issue 0230).
- `portal`: enter the level, then leave it for its homeworld through the game's exit (Spyro 2 `0x80019928(-1)`, the
  pause menu's Exit Level; Spyro 3 WarpToLevel(0, homeworld)), whose loader places Spyro in front of that level's
  portal. Evidence and addresses: `docs/re-frontier.md` title.warp.

Shown in hidden runs (`scratch/spyro23/p2b`, `p3a`, `hops2f`, `hops3b`): seven hops each from play with a camera turn
and in-betweens pending, across every homeworld, all arrive; Idol Springs and Cloud Spires `portal` land in front of
their portals. An arrival the game follows with its own cutscene or intro (Spyro 2's first Idol Springs exit plays
cutscene 72) stops there; Start skips it.

## Remaining
Spyro 2's boss arenas (ids ending in 6) and Spyro 3's slot-8 levels and Super Bonus Round have no `portal`: the game
leaves them through another load and arrives elsewhere. Neither executable names its NPCs or checkpoints, so NPC
arrivals stay `moby-teleport <class> <n>`.
