# 0230 — A terrain in-between reads level data the game has since overwritten

Status: fixed (2026-11-04). Owner: `game/render/terrain/terrain_state_producer.*` (`TerrainStateProducer::composition`)
and `terrain_rebuild.*` (`Rebuild`, its read overlay); the landing count is `spyro::Session::discLandings`.

## Observed
Spyro 2, from play with a camera turn, a level change through the storybook load `0x80019230(0)` while preseq held
in-betweens pending aborted in `Rebuild::emit`: `[packet-chain:error] packet 0x8AB94EB8 ... not a polygon`
(`scratch/spyro23/repro2`..`repro5`, `walk2b` logs, on the build of 2026-11-03).

## Cause
`TerrainStateProducer::render` re-ran the drawer over the later field's saved reads only. At the in-between camera
the drawer reads words neither real field read: the draw, colour and texture words of polygons both fields culled
(backface, outcode, fade depth), and every word of a sector only the earlier field saw. Measured on Zephyr with an
L1 turn: 3 to 58 such words in 69 of 810 rebuilds. They fell through to live guest memory, which by the time a
present composes may already hold the next load's data.

## Fix
- Each field saves its reads in address order (`drawField`), and the rebuild overlays both fields' reads, the later
  field's value winning (`FieldInputs::earlierReads`, `laterReads`). `Rebuild::unsavedReads` counts the words it
  read that neither field saved.
- Every disc read that lands in guest RAM advances `Session::discLandings`: Spyro 2/3's stock `CdRead`
  (`BootPrefixRuntime::stockCdReadLanded`) and Spyro 1's archive transfer (`ArchiveTransfer::read`). Each field saves
  the count (`FieldState::discLandings`).
- `composition` refuses the interval when the rebuild read an unsaved word and a disc read has landed since the
  earlier field, so that present shows the real field whole instead of a mix of two loads; `render` emits the same
  rebuild. A word the guest itself rewrites (animated level words) does not refuse anything.
- Limit: a guest copy of data it read from disc before the earlier field, into level memory after it, would not
  advance the count. No Spyro 2/3 level load seen does that: their level data lands by `CdRead` in place
  (`0x80084A64..` for Zephyr and Fracture Hills).

## Shown (hidden, unpaced, ires=0, fps60=1, 3840x2160; `scratch/spyro23b/tools/load0230.py`)
- The race, reproduced: Zephyr, L1 held 33 frames, then `0x80019230(0)` from play (stream step cleared and level
  0x2C set, as the guest's callers do; issue 0232). The storybook overlay lands at `0x8007FA64..0x80096264`; the
  pending Zephyr in-between at t=0.5 then reads 12 words neither field saved, nine of them inside the landed range
  (`0x80092AC4` live `0x52B94EB8`, whose low 24 bits are the packet address `0x8AB94EB8` the original abort walked to). It is held
  (`[terrstate] held a terrain in-between at t=0.5: 12 level words ...`, `[compose] ... in-between shown whole`), the
  arrival in Fracture Hills has no error (`scratch/spyro23b/f33/run.log`; `f33/load-held.png`: turning presents, the
  held one showing the real field, then the storybook). Turn 36 frames: 16 words, held. Turns 31, 32, 34, 35: the
  post-landing in-betweens read only saved words and compose.
- No holds in play: no disc read lands during a 600-frame L1 turn in Spyro 2 Aquaria Towers (water) or Spyro 3
  Seashell Shore (water), and no interval is held (`scratch/spyro23b/w2`, `w3`).
- Credits cheat (pause, then the cheat at `0x80064F14` entry 1), credits to their return `0x8001A9B0` →
  `0x80019230(0)`: back in Zephyr with no error (`scratch/spyro23b/fc/run.log`). The credits fade to black before the
  load, so this route has no terrain in-between pending at the landing.
- Not shown: the original abort as a before/after on one build (the old code is not rebuilt); pacing in real time;
  Spyro 1, which shares the producer and counts its archive transfers the same way.
