# 0226 — Spyro 1 dragon arrivals are numbered, not named

Status: open (2026-11-03). Owner: `titles/spyro1/core/debug/debug_menu.cpp:spyro1::DebugMenu::worlds`.

## Observed
Each level's dragons are `dragon 1..n` in moby-array order; n is `g_abLevelTotalDragonsTable` (`0x8006F414`). The
names exist (`g_apDragonNames`, `0x8006F694`, 79 entries) but are indexed by the dragon moby's id (props `+0x38`), which
is level data: `warp Artisans/Artisans/dragon 1` rescues NESTOR, index 3, not the first of the table.

## Next
Name a dragon arrival only from a sourced level-to-dragon-id mapping (the level WAD's moby records, read without loading
the level, or a guest table that maps them). Until then the rescue banner names the dragon on arrival.
