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
Spyro 2 from boot, `ires=0` (8x, 5472x1920 dumps), `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160`, unpaced,
dumps of fences 900-1200 (`scratch/filed-defects-2/0220/judge/run.sh`). The in-between at 1065 measures 44.7 against
the previous real present and 28.5 against the next (share 0.61), the same as the ordinary in-between at 1068
(44.1 / 28.8). The before-fix 65.3 / 4.5 was measured at 1x and was not repeated at 8x. After issue 0221's
fix (same settings, `scratch/fix-0221/`) the switch's in-between measures 0.52, and 96 of the 100 in-betweens lie
0.38-0.63 of the way between their neighbours, 1092 among them; the others are Spyro's eye blink (1095 at 0.30,
1101 at 0.74) and two still presents (`held.txt`). The `docs/media/spyro2-interpolation.webp` clip (fences
981-1137) is rebuilt from those dumps and spans the switch and fence 1092.

Spyro 1's actors take the same `across` (`titles/spyro1/render/actor_between.cpp`). In the pause report's replay
(`20261023-181349`, same settings) the one bridged switch, mesh 1 at fence 337 (`24230505`/32 to `36350404`/0), is
its largest move in the clip: the horn moves 11.5 target pixels and the in-between sits 0.67 of the way, the wing
0.65 (feature centroids, `scratch/s1-switch/track.py`, `track.txt`, `switch337.png`). Ordinary in-betweens with a
horn move over 4 pixels sit 0.17-0.74, so the switch lies inside the motion's own spread and does not snap; the
whole-frame share (0.57 among 0.32-0.68, `scratch/review-fix-3/s1/`) cannot separate halfway from slightly off.
