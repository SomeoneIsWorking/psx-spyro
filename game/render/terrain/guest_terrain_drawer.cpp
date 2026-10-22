#include "render/terrain/guest_terrain_drawer.h"

#include "core.h"
#include "lucent/log.h"
#include "render/field/guest_view_slope.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>
#include <vector>

namespace spyro::guest_terrain {
Drawer::Drawer(Core &core,
               const TerrainImage &image,
               const psx::present::EmitMemory &memory,
               FrameMode mode,
               const InBetweenWindow &inBetween)
    : core_(core), image_(image), memory_(memory), mode_(mode), inBetween_(inBetween) {
  if (mode == FrameMode::RealField && inBetween.camera != nullptr) {
    lucent::error(
        "terrdraw",
        "refusing a real field with an in-between camera: the guest's own field reads the "
        "guest's own camera words, and a copy of them could not stay equal to bytes the "
        "guest can also change");
    std::abort();
  }
  if (mode == FrameMode::RealField && inBetween.arenaBase != 0) {
    lucent::error(
        "terrdraw",
        "refusing a real field with a packet arena of its own: the guest's field allocates out "
        "of the guest's own cursor, and a window of its own would put its packets somewhere the "
        "guest never looks");
    std::abort();
  }
}

const TerrainFrame &Drawer::run() {
  const bool real = mode_ == FrameMode::RealField;
  const Facts &facts = image_.facts();
  const guest_render_globals::Globals &globals = image_.globals();
  // The borrowed registers are spilled to the save area, which is guest RAM the level's own code
  // reads back at the drawer's own exit.
  if (real) {
    guest_render_globals::spillBorrowedRegisters(core_, globals);
  }
  frame_.emplace(core_, facts, globals, presentedBounds(core_, facts), memory_, mode_);
  TerrainFrame &frame = *frame_;
  frame.inBetweenCamera = inBetween_.camera;
  frame.inBetweenArenaBase = inBetween_.arenaBase;
  frame.inBetweenPacketBudget = inBetween_.packetBudget;
  frame.linkedBins = inBetween_.linkedBins;
  frame.argument = real ? core_.r[4] : inBetween_.argument;
  frame.scratch = image_.windows(core_).scratch;
  image_.drawPasses(frame);
  if (real) {
    // The exit: retail reloads the borrowed registers from the save area, which the native passes
    // never changed, and returns with whatever its last pass left in v0 and v1. The cursor word is
    // where the next draw allocates from.
    core_.mem_w32(globals.primitiveCursor, frame.primitive);
    core_.r[2] = frame.v0;
    core_.r[3] = frame.v1;
  }
  return frame;
}

void draw(Core &core, const TerrainImage &image, psx::present::LevelReads &reads) {
  const FieldWindows windows = image.windows(core);
  // The real field's packets run from the guest's cursor to the arena's end.
  const std::uint32_t cursor = core.mem_r32(image.globals().primitiveCursor);
  const std::uint32_t arenaEnd = windows.arenaEnd;
  std::array<GuestSpan, std::tuple_size_v<decltype(windows.owned)> + 1> working{};
  std::copy(windows.owned.begin(), windows.owned.end(), working.begin());
  working.back() = GuestSpan{cursor, cursor < arenaEnd ? arenaEnd - cursor : 0u};
  const psx::present::EmitMemory memory(core, reads, working);
  Drawer(core, image, memory, FrameMode::RealField).run();
}

} // namespace spyro::guest_terrain
