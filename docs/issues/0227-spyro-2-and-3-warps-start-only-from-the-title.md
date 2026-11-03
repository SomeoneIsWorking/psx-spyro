# 0227 — Spyro 2 and 3 warps have only level starts, applied from the title screen

Status: closed (2026-11-04; reopened by review the same day, closed again). Owner: `titles/spyro2/core/warp.*:spyro2::Warp`, `titles/spyro3/core/warp.*:spyro3::Warp`.

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

## Reopened: a warp from Glimmer lost its level
Spyro 2 new game, Start skips into Glimmer (11), `warp 0/3/1` (Colossus `portal`): the log said level 13 was entered,
but the game played cutscene 72 and resumed in Summer Forest (10); level 13 never loaded, and the `portal` placement
(gameplay in 13, then Exit Level) stayed armed in `spyro::Warp`, so the next visit to Colossus would have thrown the
player out at once (`scratch/spyro23-review/g2`).

Cause: the state-7 load (`0x80071BD4`, level overlay) checks at `0x80071D98` for the level index `0x80066F54` = 1
(Glimmer) with the story byte `0x8006B084` clear and then calls the storybook load `0x80019230(4)` (table `0x8006323C`:
cutscene `0x48`, then Summer Forest), ignoring the destination `Warp::changeLevel` wrote. `spyro::Warp::applyArmed`
dropped its level after one change and never checked where play resumed.

Fix: `spyro::Warp` keeps the level until gameplay is in it (`playing`, Spyro 2/3: game state 0 and the level id) and
enters the next change from wherever the game resumes; four changes without arriving drop the warp with an error.
The placement waits for that arrival and is dropped, with a warning, when play is in another level. The same route
replaces the homeworld two-step: Spyro 2 enters a homeworld from one of its own levels (Exit Level as the game takes
it) or first enters its lowest level with a portal, and a boss arena from its homeworld through the boss door
`0x8001A290` (state 8, destination homeworld + 0x10), since an arena loaded as an exit destination stalls in state 7
phase 3.

Shown (`scratch/spyro23/r2/rt1`): from Glimmer, change 1 plays cutscene 72 into Summer Forest, change 2 enters
Colossus, the placement leaves it and Spyro stands in front of Colossus' portal. `r2/rt4`: Summer Forest to Crush's
Dungeon through the boss door, out to Idol Springs, to Shady Oasis, to Winter Tundra (two changes).

A `portal` taken from Glimmer itself before the story plays leaves Glimmer through the story and arrives at the story's
Summer Forest start, not at Glimmer's portal.


## Walks after the fix (hidden, unpaced, ires 0, fps60, 3840x2160)
`scratch/spyro23/r2/walk2b` (Spyro 2) and `walk3c` (Spyro 3), each from boot with Start skips into gameplay, then five
warps across the homeworlds: Spyro 2 Colossus `portal` from Glimmer (story route, two changes), Autumn Plains, Crush's
Dungeon (three changes, boss door), Canyon Speedway `portal`, Zephyr; Spyro 3 Seashell Shore `portal`, Enchanted
Towers, Fireworks Factory `portal`, Super Bonus Round, Midnight Mountain. Every arrival reached its level (log `level N
reached`), no fault (`r2/arrivals.png`). After each, 240 steps of settle, then a held turn (Spyro 2 L1, Spyro 3 left;
L1 does not turn Spyro 3's camera) over eight presents: every present composed, alternating `t=0.5`/`t=1` with n=2
(`<arrival>-seq/presentseq.txt`), and the far scenery (rows 40..140 of 540) steps evenly across in-betweens and real
presents on all ten, e.g. Spyro 2 Autumn Plains 15.5/15.5/15.5/14.5/14.5/14.25/13.75 px, Spyro 3 Seashell Shore
13.75/13.75/14.5/15.5/16.5/16.5/17.5. Pacing not judged.
