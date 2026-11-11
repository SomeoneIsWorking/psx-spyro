# 0240 — Spyro 2 and 3: a t = 1 gameplay replay differs from its real draw

Status: closed (2026-11-11), shown fixed in frames. Owner: psxport `Gp0RecordTap`; `game/render/field/keyframe_blend.cpp`.

## Observed
Warp `0/1/0`, Up+Left held, judgement settings, hidden, unpaced, a local t = 1 build (a copy of the tree with
`t = 1.0f` in `ReplayFrame::beforeDraw`, `psx-spyro` `scratch/review6/hc`, not shipped): every in-between differs from
the real present after it (`scratch/review6/nullcase.txt`).
- Spyro 2 (`t1-s2diff.png`): the yellow sparks around Sparx are drawn a fraction of a native pixel off their real
  place, as Spyro 1's 0239; one small spot on a far moby.
- Spyro 3 (`t1-s3diff.png`, `t1-s3spot.png`): Sparx's sparks the same way, and at about (3800, 650) at 8x a
  peach terrain sliver behind a dark wall's edge is cut differently: the replay shows a narrower strip.
- The same boxes with `PSXPORT_DEPTH=0` (`t1-s3spot-nodepth.png`), so native depth does not cause them.

## Cause
Two causes. Sparx's sparks are 0239's: the record tap drew GP0 lines and sprites without their words' places
(`psxport` `gpu/gp0_record_tap.cpp`), while the replay's decode placed them; the Spyro 3 sliver was in the same boxes
and is gone with it. Separately, at a moby's animation switch (Spyro 3's attract credit sign, every 60 ticks)
`game/render/field/keyframe_blend.cpp:across` returned at t = 1 the cross pair's last step (`from`'s frame A to `to`'s
nearer frame), not `to`'s own pose, so the sign's top drew up to a pixel off.

## Fix
psxport `0b42f751` (places on real lines and sprites); `across` returns `to` at t = 1.

Evidence (`scratch/null1`, judgement settings, depth on, hidden, unpaced; a local probe forcing t = 1 in
`ReplayFrame::beforeDraw` and comparing each assembled in-between's primitives, places included, with the real record
it precedes; never committed): warp `0/1/0`, Up+Left held 1500 fields after boot and the attract, every compared
in-between identical, Spyro 1 2863, Spyro 2 1243, Spyro 3 1061 (`Lspyro{1,2,3}.log`), and 20 of 20 dumped
in-betweens per title pixel-identical to the real present after them (`Lspyro{1,2,3}/`). Spyro 3's attract at the
credit sign's switch, rec 614 (`t3e_614.png` before; `t3f.log` after: 146 of 146 identical).
