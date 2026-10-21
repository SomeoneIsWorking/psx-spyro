# 0207: Spyro 2 and 3 front-end frames have no producers

Status: partial — the title screens, the loading pictures and the Spyro 3 demo compose; the fade seam
before a load, Spyro 3's pause menu and Atlas stay whole (below)

## Observed
psx-spyro 0196 pass 6, fps60, 16:9, hidden window, real speed, `warp <level>` from the title screen
(`scratch/judder-pass6/scan-spyro{2,3}-*`, `s2-shot`, `s3-shot`, `judge-s2`, `judge-s3`):
- Spyro 3, game state 5 (the "THE ADVENTURE CONTINUES" screen, `s3-shot/at2.png`): 31 logic frames of
  3 fields, one of 4 and three of 5 on every route; every in-between logs `shown whole: no state saved`.
- Spyro 2, game state 5, the fade off the title scene before the warp's load (`s2-shot/at0.png`): six
  frames of 5 fields and one of 3, `no state saved`. The title scene's 2-field frames before it refuse with
  `entry 1474 unkeyed` (issue 0179).

## Reproduction (2026-10-22)
`scratch/inbetween-gaps/walk.py <title> <tag>` from boot at real speed with the player's settings copied
(ires 0, fps60 1, 16:9), `PSXPORT_PRESENT_SINK=3840x2160`, hidden window, no `PSXPORT_NOPACE`; `gaps.py`
splits the presents by game state and simulates a 60.00 Hz display. Before the changes below:
- Spyro 2 (`IDLE=40 NEWGAME=1`, load ~1): game state 11 (intro cutscene, title, menus) 877 of 1200
  in-betweens whole, `entry N unkeyed`; the loading storybook's fade-in (game state 5) 46 whole,
  `no state saved`.
- Spyro 3 (`IDLE=40`, load ~0.9): title 864 of 1214 whole, `entry N unkeyed`; the attract demo's
  gameplay (game state 0) every in-between whole, `entry N unkeyed`; game state 5 307 whole.

## Cause
- `game/render/field/field_redraw.cpp:fieldClaim` claimed only the packets linked before the
  dispatcher's letterbox, so the title overlay's logo, text and menu sprites (Spyro 2 `0x8007BFC4` from
  `FUN_80015560`; Spyro 3 `0x80077CFC`, called by the draw `0x8001E638` in state 11), linked after the
  dispatcher returns, stayed unkeyed and showed the field whole. Spyro 3's gameplay draw `FUN_8001D274`
  links the attract demo's DEMO MODE box and text the same way.
- The loading screen's picture had no producer: Spyro 2's `FUN_800151F0` (three storybook pages) and
  Spyro 3's `0x80059038` step 0 (four pages through `FUN_80059358`) draw the pages at brightness
  `fade << 4` up to 0x80 and step the fade once per logic frame.
- Redrawing `FUN_8001D274` whole stopped with `budget-exhausted` in `FUN_8001C368` on demo frames:
  `psxport/runtime/cpu/execution_exit.cpp:ExecutionBudget::currentTurn` gave the dispatcher's original
  call inside a `GuestRedraw` one field of cycles, and a redraw cannot suspend at a turn's end.

## Fix
- `field_redraw::ScreenDraw` (`Facts::screenDraws`): a game state's draw that calls the dispatcher and
  links its 2D after it is redrawn whole at each in-between; the dispatcher inside it only draws, and the
  flatten claims everything the frame linked. Spyro 2 `0x80015560`; Spyro 3 `0x8001D274` and the title
  overlay's `0x80077CFC`. An overlay-held draw names the overlay's content identity (Spyro 3's title
  overlay, LBA 570, 27 sectors at `0x800742D0`, SHA-256 `675044820f58e521...`) and is installed by
  `engine_render::screenDrawsLanded` from `BootPrefixRuntime::codeLanded` each time that overlay lands.
- `game/render/field/loading_picture.*`: the loading screen's draw keyed to its fade counter while the
  pages show; its state is the brightness and the packets it linked; the render walks the pages at the
  brightness between two saves and the text after them unchanged. Facts per image in
  `titles/spyro{2,3}/render/render_facts.h` (Spyro 2's pages in the overlay pair, Spyro 3's in bin 0).
- psxport `ExecutionControl::Unsuspendable`: while a `GuestRedraw` runs, every call gets the run's
  budget in place of a turn.

## Proof (2026-10-22)
- Spyro 2, `s2-walk3`, load 0.81–1.35: title and menus 1200/1200, new game 97/97, press start 54/54;
  the storybook fade composes (46 → 0 whole); conversation (game state 6) 1795/1795; gameplay 584/584.
  Stepped 8x capture (`s2-book`): book in-betweens at 0.42–0.50 of the way between their frames' mean
  brightness over seven frames. Title at 8x (`s2-title8x`): halfway 0.65–0.93 over 21 moving blocks,
  logo and Press Start present in every in-between.
- Spyro 3, `s3-walk5`/`s3-warp`, load 1.3–3.1: title 1098/1098; demo gameplay 576/576 (was 0);
  `warp 11` gameplay 560/560 plus play and turn phases all composed; the loading picture's fade 31
  composed. Stepped capture (`s3-gs5`): fade in-betweens at 0.41–0.50 of the way between their frames.

## Open
- **The fade before a load (Spyro 2).** `FUN_8004D810` copies the shown picture into the draw buffer
  and darkens it over 16 one-field presents with a subtractive tint `FUN_8004D748`; its first present
  follows a 5-field stall and shows whole (`no state saved`), so the first darkening step lands at once
  (mean grey 88 → 60, then about one step per field). Seen at every load (records 1886, 3059, 4282 in
  `s2-walk3`). Proper fix: one owner for the fullscreen tint shared by Spyro 1 `0x800190D4` and
  Spyro 2 `0x8004D748` (lift `spyro1::FullscreenTint` into `game/render`), whose rest state of colour 0
  composes the copied picture with the tint at t. Not done this pass: it moves Spyro 1's
  `screen_overlays`, already changed in it. The same owner would take the dispatcher's own fades
  (Spyro 2 `0x8004D748` at `0x80066F68`/`0x80067150`, Spyro 3 `0x800200A0`), which the redraw draws at
  the next frame's level, so the fade-in after each load steps at 30 fps (issue 0196, pass 7).
- **Spyro 3 loading screen after the fade** (`0x80059038` steps 1, 2 and 4, about 280 in-betweens per
  load): shown whole (`no state saved`), but the picture is still — 30 consecutive stepped presents
  identical (`s3-gs5`). Its draw calls the overlay's `0x80075114` after a `VSync(0)` of its own, which
  is where its 3-field frames come from. No visible gap.
- **Spyro 3 pause menu** (game state 4): Sparx's cursor is drawn by the second moby pass the draw
  `0x8001E638` chains after the flatten (`kSecondPassFlattenReturns`), unkeyed, so in-betweens show
  whole (77 in `s3-atlas`). Proper fix: key the second pass to a producer of its own.
- **Spyro 3 Atlas** (state 4, steps 0x14–0x18, `FUN_80058408` and the menu `FUN_80020D70`): the opening
  book and Sparx moving over the contents show whole, `no state saved`. Needs `FUN_80058408` traced
  (the book's open angle and the page state) for a producer.
