# 0207: Spyro 2 and 3 front-end frames have no producers

Status: open

## Observed
psx-spyro 0196 pass 6, fps60, 16:9, hidden window, real speed, `warp <level>` from the title screen
(`scratch/judder-pass6/scan-spyro{2,3}-*`, `s2-shot`, `s3-shot`, `judge-s2`, `judge-s3`):
- Spyro 3, game state 5 (the "THE ADVENTURE CONTINUES" menu over the homeworld flyby, `s3-shot/at2.png`):
  31 logic frames of 3 fields, one of 4 and three of 5 on every route; every in-between logs
  `shown whole: no state saved`, so the flyby moves at 20 fps (12 fps on the 5-field frames). Its 2-field
  frames show whole as well.
- Spyro 2, game state 5, the fade off the title scene before the warp's load (`s2-shot/at0.png`): six
  frames of 5 fields and one of 3, `no state saved`. The title scene's 2-field frames before it refuse with
  `entry 1474 unkeyed` (issue 0179).

## Cause
The fields are the guest's own: Spyro 3's update 0x80055400 (called from game main 0x8001200C) runs
about 1.31M guest cycles on that menu, so the draw's `while (VSync(-1) - last < 2)` wait ends on the third
field. Nothing composes them because `spyro::field_redraw` is bound to the gameplay dispatcher 0x8001E460,
which the draw 0x8001E638 calls only in states 2 and 0x12; state 5 draws through 0x80059038, which no
producer covers. Spyro 2's draw for its state 5 is not yet traced.

## Proper fix
A producer (or the field redraw) on Spyro 3's 0x80059038 and on Spyro 2's state-5 draw once traced, keyed per object so an
in-between moves the flyby camera and its objects.
