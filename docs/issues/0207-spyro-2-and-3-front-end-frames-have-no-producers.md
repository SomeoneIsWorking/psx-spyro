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
  in-betweens whole, `entry N unkeyed`; in the walk's press-cross segment (the menus, the fades before
  each load and the storybook's two-field frames before game state 5 is first sampled) 46 whole,
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
  the press-cross segment 46 → 12 whole (34 in-betweens the loading picture now composes; the 12 left
  are the fade-to-black seams, open below); game state 5's own 2 in-betweens composed; conversation
  (game state 6) 1795/1795; gameplay 584/584. Correction (review): at real speed most of the storybook
  fade runs one present per field (`fields=1/1`, 192 presents in the first load of `rv-s2`), which has
  no in-between to compose; an earlier line here said the fade went 46 → 0 whole, which mixed the two
  segments. The 8x figure (`s2-book`, in-betweens 0.42–0.50 of the way between their frames' mean
  brightness) was REPL-stepped and its captures are deleted, so it is unverified. Title at 8x
  (`s2-title8x`): halfway 0.65–0.93 over 21 moving blocks, logo and Press Start present in every
  in-between.
- Spyro 3, `s3-walk5`/`s3-warp`, load 1.3–3.1: title 1098/1098; demo gameplay 576/576 (was 0);
  `warp 11` gameplay 560/560 plus play and turn phases all composed; the loading picture's fade 31
  composed. Stepped capture (`s3-gs5`): fade in-betweens at 0.41–0.50 of the way between their frames.
- Re-landing (review): each landing of the title overlay removes the screen draw's override keyed to
  the image it replaced and installs one on the new image, keyed by the content
  `publishStockReadLanding` published (no second hash). `fx-s3w` (debug `cd`): the overlay at
  `0x800742D0` landed 4 times from boot to the warp, title 240/240 composed. `fx-s3` (100 s idle, two
  attract demos and back): title 1415/1415 and demo gameplay 640/640 composed, load 0.39-0.69.

## Open
- **The fade before a load (Spyro 2)** — fixed, pass 9 (issue 0196). Cause:
  `game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::stepFrame` deferred the VSync fields
  of `FUN_8004D810`'s loop, which swaps and draws one frame per field, as waits inside a logic frame, so
  3 iterations (16 in boot) were merged into one record and the tint was ambiguous. `fadeFrameWait`
  (Spyro 2 `0x8004D918`, Spyro 3 `0x80020270`) presents each one. The tint and letterbox now have one
  owner, `game/render/screen_overlays.*`, shared by all three images.
- **Spyro 3 loading screen after the fade** (`0x80059038` steps 1, 2 and 4, about 280 in-betweens per
  load): shown whole (`no state saved`), but the picture is still — 30 consecutive stepped presents
  identical (`s3-gs5`). Its draw calls the overlay's `0x80075114` after a `VSync(0)` of its own, which
  is where its 3-field frames come from. No visible gap.
- **Spyro 3 pause menu** — fixed, pass 9: the second moby pass (walk return `0x8001EA28`, drawer
  `0x80031124`, flatten return `0x8001EA40`) is redrawn in the field redraw and owned in its own depth
  table `kSecondPassTable` (`p9-s3pause`: all composed, Sparx 0.62–1.59 of halfway). The 3D counters
  along the top of the pause screen still equal the next frame in every in-between (30 fps); their
  drawer is not traced (`FUN_8001EDEC` is not it).
- **Spyro 3 cutscene cameras** — fixed, pass 9: the overlay's blend at `0x80077E0C` uses Spyro 2's
  rule, so `scriptedCamera` facts (state 6, `0x8006E020`/`0x8006E03C`) make each shot change a cut.
- **Spyro 3 Atlas** (pause steps 0x14–0x18 of `0x8006FBC8`, `FUN_8001DD1C` → `FUN_80058408` then
  `FUN_80020D70`): every opening in-between is shown whole, `entry 1 unkeyed`, the first packet of the
  buffer (`p9-s3atlas`: 11 of 11, brightness and extent step every second present). `FUN_80058408`
  writes at the cursor `0x8006C664` outside any scope: a black TILE at (0x80,0x41) 0x100×0x80, a textured
  quad of the copied page through `FUN_800289C8` (tpage word 0x168), then the sparkles `FUN_8002ECA8`.
  Both packets are constant, so what grows and brightens is drawn by the menu `FUN_80020530` (table
  `0x80067570` indexed by `0x8006FBD0`), not yet traced. Proper fix: key `FUN_80058408`'s packets as
  constant elements and give the menu's opening its producer from the words it reads.
