#include "render/terrain/guest_terrain_drawer.h"

#include "core.h"
#include "render/field/guest_view_slope.h"

namespace spyro::guest_terrain {

const TerrainFrame &Drawer::run() {
  const Facts &facts = image_.facts();
  const guest_render_globals::Globals &globals = image_.globals();
  // The borrowed registers are spilled to the save area, which is guest RAM the level's own code
  // reads back at the drawer's own exit.
  guest_render_globals::spillBorrowedRegisters(core_, globals);
  frame_.emplace(core_, facts, globals, presentedBounds(core_, facts), memory_);
  TerrainFrame &frame = *frame_;
  frame.argument = core_.r[4];
  frame.scratch = image_.scratch(core_);
  image_.drawPasses(frame);
  // The exit: retail reloads the borrowed registers from the save area, which the native passes
  // never changed, and returns with whatever its last pass left in v0 and v1. The cursor word is
  // where the next draw allocates from.
  core_.mem_w32(globals.primitiveCursor, frame.primitive);
  core_.r[2] = frame.v0;
  core_.r[3] = frame.v1;
  return frame;
}

void draw(Core &core, const TerrainImage &image) {
  const psx::present::EmitMemory memory(core);
  Drawer(core, image, memory).run();
}

} // namespace spyro::guest_terrain
