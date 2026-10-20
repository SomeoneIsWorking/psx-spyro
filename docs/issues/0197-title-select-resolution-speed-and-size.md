# 0197: title select is low-resolution, slow and wrongly sized

Status: open

## Observed
USER, 2026-10-20: "title select is bad (ires, speed, size)".

## Known owners
- Speed: psxport 0162. A panel showed a retained copy refreshed on a gate (about 2 Hz visible).
- Resolution and size: psxport 0163. The selector cover-cropped and magnified each panel's picture,
  so the internal-resolution setting had no visible effect.
- The record raster's CPU cost scales with ires (psxport 0161 and the ires work that follows it), so
  three panels at a high internal resolution cost three times that.

Both fixes and the launcher's fullscreen default (`tools/run.py`) are uncommitted as of this issue.
Close this only after they land and the selector is checked in a real window at the player's display
resolution.

## Not integrated
USER, 2026-10-20: "title select currently is an external thing, which is obvious like it's not part of
the game, RmlUi doesn't work there for example. It should be more integrated into the engine".

The selector is a host layer drawn outside a game session (`psx::host::PickerSession` and
`PickerComposite`), so the in-game RmlUi menu, settings and input are absent there. The fix is that the
selector becomes a screen of the product, presented through the same path as a game frame: the same
present path, overlay, menu and input owners, with the panels as that screen's content.
