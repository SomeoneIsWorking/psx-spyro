// guest_moby_drawer.h — the moby drawer of this engine family (SCUS_944.25 0x80044504, SCUS_944.67
// 0x80031124), which draws the render list the visibility walk just wrote, and what a widened view
// adds after it.
//
// The guest's call draws retail's list. At a widened aspect the field's object pass (8004C534 /
// 8001EC5C) then draws the margin: inside a rolled-back run the walk lists only the mobys wholly
// past the 4:3 window (guest_moby_visibility.h), over the list region the drawer just consumed, and
// the original drawer draws them into the idle draw buffer's pool (DrawSection::idlePool), so the
// guest's cursor and the pool room it tests stay retail's. The run keeps what the frame needs of
// it: those packets, the ordering-table bins and the packets they link after, and the far-bin mark
// the flatten reads (SCUS_944.25 8001B2B8). Every other byte, the cursor, class bytes and lists
// among them, goes back. Last, the title intro card's extension (intro_card.h).
#pragma once

#include "core.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/field/intro_card.h"

#include <cstdint>

namespace spyro::guest_moby_drawer {

struct Facts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // The return address of the field object pass's call (SCUS_944.25 jal at 8004C564, SCUS_944.67
  // at 8001EC8C); Spyro 3's Sparx pass (8001EA28) draws no margin.
  std::uint32_t fieldPassReturn = 0;
  intro_card::Facts introCard;
};

// One image's words the drawer and its margin pass reach.
struct Image {
  const Facts &drawer;
  const guest_moby::Facts &walk;
  const guest_render_globals::Globals &globals;
  std::uint32_t sectorTable = 0; // the word holding the level's terrain sector table
};

void draw(Core &core, const Image &image);

} // namespace spyro::guest_moby_drawer
