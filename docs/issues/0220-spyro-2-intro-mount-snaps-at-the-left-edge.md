# 0220 — Spyro 2 intro: Ripto's mount snaps at its in-between leaving the left edge

Status: fixed 2026-10-29. Owner: `game/render/field/keyframe_blend.*` (`across`), called from
`field_redraw.cpp:recordBetween`; the layout check is the moby walk's (`guest_moby::slotsShareLayout`).

## Observed
Spyro 2 boot intro, 16:9, `fps60=1`, unpaced, dump from fence 900. At fence 1065 the in-between draws
the mount's tail almost at the new frame's pose (mean difference 0.23 to the new frame vs 2.14 to the
previous one). The present is composed with no cut (`[presentseq] fence=1065 ... composed=1 cut=0`).

## Cause
The mount's nine records (array 0x80093BCC, meshes 0-8) switch animation between fields 583 and 584:
keyframes `1E1D0303` progress 0x08 (slot 3, frames 0x1D to 0x1E, half way) become `01000404` progress
0 (slot 4, frames 0 to 1). `keyframe_blend::between` answers a pair that does not continue with `to`,
and `across`, which bridges a switch, was only used by Spyro 1's actors. It also bridged from `from`'s
nearer frame (0x1E), which this clip ends on and slot 4 starts from, so even a cross-slot blend
landed on the new pose.

## Fix
`across` takes `from`'s frame A as A, `to`'s nearer frame as B, and carries `from`'s own share the
rest of the way at `t` (0x1D at 12/16 for this switch), keeping the nibble form's step row.
`recordBetween` uses it for a switch when slot A's and slot B's meshes count the same vertices, the
condition the drawer (0x80044504) relies on when it walks frame B's stream with slot A's count.

## Check
`scratch/filed-defects-2/0220/`: the probe reads the switch per field. With the same unpaced run, the
in-between at 1065 measures 43.7 against the previous frame and 27.6 against the next, in line with
the ordinary in-between at 1068 (42.5 / 27.4); before the fix it measured 65.3 / 4.5. All 100 in-betweens
from fence 900 to 1200 lie 0.3-0.7 of the way between their neighbours (`clip/held.txt`). The
`docs/media/spyro2-interpolation.webp` clip spans the switch.
