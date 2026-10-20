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
