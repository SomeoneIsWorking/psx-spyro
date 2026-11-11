# 0239 — Spyro 1: a t = 1 gameplay replay differs from its real draw in a few yellow squares

Status: closed (2026-11-11), shown fixed in frames. Owner: psxport `Gp0RecordTap`.

## Observed
Warp `0/1/0`, Up+Left held, a local t = 1 probe (`t = 1.0f` in ReplayFrame): 6 of 6 in-betweens differ from
the real present after them only in a few small yellow squares beside a gem's sparkle at the right of the
screen (`psx-spyro` `scratch/camfrac/t1turn.log`, `t1diff.png`; boxes such as (4784, 904)-(5141, 1009) at 8x).
The rest of the frame is identical. The same frames with the camera matrix write disabled are
pixel-identical to these, so the camera track does not cause it. Title-screen replays at t = 1 are exact.

Re-run 2026-11-11 (`scratch/review6/`, `t1turn.log`, `nullcase.txt`, `t1diff.png`), with whole angles on the
guest's own matrices: 6 of 6 differ, all in the same place, about 4480-5130 x 1070-1310. Each differing square
is one of the yellow sparks around Sparx drawn a fraction of a native pixel (2 to 4 output px) off its real
place, same size and colour; nothing else in the frame differs. Spyro 2 and 3 show the same with Sparx's sparks
(0240), so the cause is likely shared.

## Cause
`psxport` `gpu/gp0_record_tap.cpp:Gp0RecordTap::executeCommand` decoded a line packet's ends, and `recordSprite` a
sprite's origin, without the places stored with their words, while the replay's capture decodes the same words with
them (`present/packet_chains.cpp`, `decodePacketPrimitive`). The sparks are GP0 lines whose first end the drawer
stores from the GTE (Spyro 1's sparkles `0x80077108`, natively through `storeVertex`; Spyro 2/3's particles
`0x80048128` and glows `0x8004AC78`), so a real present drew them on their whole pixels and a replay a fraction off.

## Fix
psxport `0b42f751`: the tap places line ends, poly-line points and sprite origins through the decode the replay uses.

Evidence (`scratch/null1`, judgement settings, depth on, hidden, unpaced; a local probe forcing t = 1 in
`ReplayFrame::beforeDraw` and comparing each assembled in-between's primitives, places included, with the real record
it precedes; never committed): warp `0/1/0`, Up+Left held 1500 fields after boot and the attract, every compared
in-between identical, Spyro 1 2863, Spyro 2 1243, Spyro 3 1061 (`Lspyro{1,2,3}.log`), and 20 of 20 dumped
in-betweens per title pixel-identical to the real present after them (`Lspyro{1,2,3}/`). Spyro 3's attract at the
credit sign's switch, rec 614 (`t3e_614.png` before; `t3f.log` after: 146 of 146 identical).
