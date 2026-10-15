#include "guest_terrain_drawer.h"

#include "core.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_terrain_frame.h"
#include "guest_terrain_passes.h"
#include "lucent/log.h"
#include "native_execution.h"
#include "spyro_session.h"
#include "terrain_rebuild.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>
#include <utility>
#include <vector>

namespace spyro::guest_terrain {
namespace {

// The GTE register and command numbers are shared vocabulary (psxport gte_registers.h), not this
// pass's.
namespace gte = psx::gte;

// The columns the presentation shows past each side of the authored window, 0 at 4:3.
int horizontalMargin(const Core &core) {
  const GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  return plan.widescreen() ? plan.projectionHorizontalMargin : 0;
}

} // namespace

Drawer::Drawer(Core &core,
               const Facts &facts,
               const guest_render_globals::Globals &globals,
               TerrainMemory &memory,
               FrameMode mode,
               const InBetweenWindow &inBetween)
    : core_(core), facts_(facts), globals_(globals), memory_(memory), mode_(mode) {
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
  inBetweenCamera_ = inBetween.camera;
  inBetweenArenaBase_ = inBetween.arenaBase;
  inBetweenPacketBudget_ = inBetween.packetBudget;
}

const TerrainFrame &Drawer::run() {
  const bool real = mode_ == FrameMode::RealField;
  // 80023BCE: the borrowed registers are spilled to the save area, which is guest RAM the level's
  // own code reads back at the drawer's own exit.
  if (real) {
    guest_render_globals::spillBorrowedRegisters(core_, globals_);
  }
  frame_.emplace(core_,
                 facts_,
                 globals_,
                 ScreenBounds(facts_.nativeWidth, horizontalMargin(core_)),
                 memory_,
                 mode_);
  frame_.value().inBetweenCamera = inBetweenCamera_;
  frame_.value().inBetweenArenaBase = inBetweenArenaBase_;
  frame_.value().inBetweenPacketBudget = inBetweenPacketBudget_;
  TerrainFrame &frame = *frame_;
  frame.scratch = core_.mem_r32(globals_.scratchBaseWord) - facts_.scratchListsBelowEnd;
  core_.gte().writeData(gte::kVxy1, frame.scratch);
  if (real) {
    // The cursor is the guest's own; the drawer commits `frame.primitive` back to it only at exit.
    const std::uint32_t cursor = core_.mem_r32(globals_.primitiveCursor);
    spyro::session(core_).terrainPacketBudget =
        static_cast<std::int32_t>(frame.scratch + kPrimitiveArenaBytes - cursor);
  }
  if (real) {
    // The drawer's one call out of its own body. Only the guest can answer it, so an in-between
    // field is handed the sectors its visibility was decided for, not asked again.
    {
      spyro::PreservedReturnAddress returnAddress(core_);
      spyro::callGuestJumpedFrom(
          core_,
          facts_.overrideName,
          {.jal = facts_.visibilityCallSite, .target = facts_.sectorVisibility},
          0u);
    }
    core_.mem_w32(facts_.visibleSectorCount, core_.r[2]);
    // The guest's own answer to "which sectors are visible", copied here because it exists NOW and
    // nowhere later: the classification below clears each byte as it consumes it. An in-between
    // field cannot ask the guest — there is no guest to ask, and the guest's visibility walk is
    // guest code — so it is handed this, and draws the union of two fields' answers
    // (terrain_state_producer.h).
    {
      const std::uint32_t count = core_.mem_r32(facts_.classify.sectorCount);
      std::vector<std::uint8_t> visible(count);
      for (std::uint32_t i = 0; i < count; ++i) {
        visible[i] = memory_.r8(kScratchpad + i);
      }
      spyro::session(core_).terrainVisibility = std::move(visible);
    }
  }
  classifySectors(frame);
  drawDetailSectors(frame);
  drawTranslucentSectors(frame);
  // Both split lists are ended by the translucent pass, which writes the terminating zero at each
  // cursor after its own deferrals (guest_terrain_translucent.cpp) — retail's arrangement, and the
  // only one, on either field kind.
  splitCoarsePolygons(frame);
  splitFinePolygons(frame);
  resplitOversizedPrimitives(frame);
  drawFarSectors(frame);
  if (real) {
    // SCUS_944.25 80029118: the exit. Retail reloads the borrowed registers from the save area,
    // which the native passes never changed, and returns with whatever its last pass left in v0 and
    // v1. Both are the guest's own state: the cursor word is where the next pass allocates from and
    // v0/v1 are the O32 result registers the guest's caller reads.
    core_.mem_w32(globals_.primitiveCursor, frame.primitive);
    core_.r[2] = frame.v0;
    core_.r[3] = frame.v1;
  }
  return frame;
}

std::vector<LevelWord>
draw(Core &core, const Facts &facts, const guest_render_globals::Globals &globals) {
  const FieldWindows windows = fieldWindows(core, facts, globals);
  // The real field's packets run from the guest's cursor to the arena's end.
  const std::uint32_t cursor = core.mem_r32(globals.primitiveCursor);
  const std::uint32_t arenaEnd = windows.scratch + kPrimitiveArenaBytes;
  std::array<GuestRange, std::tuple_size_v<decltype(windows.owned)> + 1> working{};
  std::copy(windows.owned.begin(), windows.owned.end(), working.begin());
  working.back() = GuestRange{cursor, cursor < arenaEnd ? arenaEnd - cursor : 0u};
  GuestMemory memory(core, working);
  Drawer(core, facts, globals, memory, FrameMode::RealField).run();
  return memory.firstReads();
}

} // namespace spyro::guest_terrain