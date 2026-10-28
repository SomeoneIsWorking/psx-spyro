# 0219 — Spyro 2 attract demo holds an in-between at each fade

Status: open. Owner: `game/core/frame_cut.*` (`FrameCut::onFrameTail`).

## Observed
Spyro 2 attract demo, 16:9, `fps60=1`, unpaced, fences 5250-5560 from boot (no input). The in-between at
fences 5265, 5380 and 5491 equals the new frame (`[presentseq] ... cut=1`), so each one shows a
whole-frame step while the camera keeps moving. 5265 and 5491 are the starts of the demo's fade to
black, and 5380 is the end of its fade-in.

## Cause
At those records, game state `0x800681C8` flips between 0 and 3 (`[cut] cut=true state=3 level=22`),
and `FrameCut` treats any game-state change as a scene change. The state-3 draw `0x80014504` draws the
same field scene (`0x800155A0(0x3D)`) through the same camera at sub-states (`0x800698F0`) 0 and 2. At
sub-state 1 it draws something else, yet the presented frame still shows the moving scene. Folding
state 3 at sub-states 0 and 2 into gameplay did not clear the cut (sub-state 1 still flips it), so it
was reverted. Spyro 3's state-3 draw `0x8001DC5C` has the same shape (field scene at sub-states
`0x8006FBC8` 0 and 4).

## Next
Find which draw renders the field during sub-state 1 (store watch `PSXPORT_CW=0x800698F0,0x800698F4`
shows 0 -> 1 -> 2 across each fade). The cut should follow whether that record's field scene and camera
continue, not the state word.
