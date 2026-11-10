# 0236 — Spyro 1 Stone Hill ram: the in-between shows the later pose

Status: open. Owner: not found yet; the moby record track (`titles/spyro1/core/frame/draw_motion.cpp:MobyMotion`)
blends correctly, so the stepping input is something the section reads outside the moby record.

## Observed
Warp `0/1/0`, Up held 30 steps, hidden, unpaced, judgement settings. The ram in front of the first wall
(crop 2600,300..2900,520 of the 5472x1920 dump) bobs its head; each t=0.5 present is close to the real present
after it rather than halfway: presents 0/1, 2/3, 4/5, 6/7 pair up (t=0.5 then t=1), with the big pose change
between the pairs. Sparx, Spyro and the gems in the same run land halfway.

## Evidence
- `scratch/replay3/walk5/ram8.png`: eight presents, t=0.5 then t=1 alternating from the top left.
- `scratch/replay3/walk6.log` (`motionrec` trace, since removed): every moby whose keyframes changed at that
  in-between is shown halfway, e.g. `0x80177960 class 120 03020404/32 -> 04030404/0 shown 03020404/48`; only one
  record in the run switched animation (`0x801737B8`, class 121, crossed). `mobys: 197 of 197 records paired`.

## Next
Find the ram's record (class and address by position against the camera) and read what its draw reads besides
the tracked words: a per-class update inside the section, a second record, or the shaded-moby queue.
