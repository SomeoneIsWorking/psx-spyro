# 0197: title select is low-resolution, slow and wrongly sized

Status: fixed headless; the real-window check (real-time pacing, the player's display resolution) is still owed

## Observed
USER, 2026-10-20: "title select is bad (ires, speed, size)".

## Known owners
- Speed: psxport 0162. A panel showed a retained copy refreshed on a gate (about 2 Hz visible).
- Resolution and size: psxport 0163. The selector cover-cropped and magnified each panel's picture,
  so the internal-resolution setting had no visible effect.
- The record raster's CPU cost scales with ires (psxport 0161 and the ires work that follows it), so
  three panels at a high internal resolution cost three times that.

Close this only after the selector is checked in a real window at the player's display resolution.

## Not integrated
USER, 2026-10-20: "title select currently is an external thing, which is obvious like it's not part of
the game, RmlUi doesn't work there for example. It should be more integrated into the engine".

The selector is a host layer drawn outside a game session (`psx::host::PickerSession` and
`PickerComposite`), so the in-game RmlUi menu, settings and input are absent there. The fix is that the
selector becomes a screen of the product, presented through the same path as a game frame: the same
present path, overlay, menu and input owners, with the panels as that screen's content.

## Cause
`psx/host/picker_session.cpp:PickerSession` drew the selector with its own pass (`PickerComposite` over
`PaneCompositor`) straight into the window, outside the host Game's present, so the overlay, the RmlUi
menu and host input never ran there. That pass cover-cropped and magnified each panel's picture, and the
running panel was read back off the GPU every 15 presents to keep a retained copy.

## Fix (psxport consolidation B3)
- The selector is the host Game's screen: `PickerPanes` lists the panes, `DisplayPresenter::buildPanes`
  draws them into the host's present image (`RenderState::buildPanes`) and `show()` presents it with the
  overlay. `PickerComposite`, `PaneCompositor` and the logo magnification are deleted.
- Each panel presents at its settled cover size (`PickerLayout::imageSize`) and is drawn 1:1, centred and
  cut by the seams; nothing is cropped to fit or magnified.
- A panel is read back only while it boots and once when it is deselected; the selected panel shows its
  live presents.
- A menu setting reaches the running panel through `Mods::reload`.
- Input goes through `host_input.h` buttons: D-pad Left/Right step the selection (they were wired to the
  L2/R2 bits, so L2/R2 moved it before). `picker shot` is gone; `pshot` captures the selector.

## Behaviour that changes on purpose
- D-pad Left/Right move the selection; L2/R2 no longer do.
- `picker shot` is removed; `pshot` captures the selector.
- The selected panel shows its live presents, fades included, instead of the last lit frame.
- A paused panel is drawn 1:1, centred and cut by the seams, so it shows a centre slice of a wider picture.
- Each panel shows the title's own black border rows as bands about 24 px tall at its top and bottom;
  the deleted content measure used to crop them, and nothing is cropped now.
- The fps60 menu row is hidden on the selector (the host Game declares no interpolation).
- `select left|right` replies `ok: step left|right` instead of `ok: panel N (slug)`.

## Proof
Headless (`PSXPORT_VK_HEADLESS=1` read from `/proc/<pid>/environ`), `scratch/b3/selector.py`, shipping
settings, captures in `scratch/b3/run/`; rerun on the committed source after the review fixes in
`scratch/b3-fix/run/` (paced selector 1, 2, 0, 2, l2/r2 no change, `select right|left` 0, 2, `pick spyro2`
boots; Spyro 1/2/3 `warp 10` gameplay in `gameplay-montage.png`):
- `selector-fast-0.png`: three slanted panels, each title's own picture at its own size, selected Spyro 1
  in colour, the others grey and dimmed, gold seams.
- `tap right` x3, `tap left`: `selected panel 1`, `2`, `0`, `2`; `tap l2`, `tap r2`: no selection change;
  `select right` / `select left`: `0`, `2`.
- `menu on`: the menu is drawn over the selector (`scratch/overlay-shot/run/selector-menu.png`; `pshot`
  now captures the window's screen pass, overlay included).
  `menu nav down`, `menu nav enter`: handled, the dump reads
  `Internal Resolution = X2` (was `Vanilla`), and the running panel's picture shows the finer raster
  (`selector-fast-after-menu.png`).
- `pick spyro2`: `spyro2 confirmed at panel 1`, its boot prefix returns and its title frame is captured.
- Paced run (`--paced`): 1437 pacing calls in 24 s settled, 59.9 Hz; every gap over 30 ms is a `pshot`
  PNG write; a selection change resumes the new panel's pacer with no gap.
