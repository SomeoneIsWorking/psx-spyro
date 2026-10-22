// guest_terrain_frame.h — the state one call of the terrain drawer carries from pass to pass: the
// registers retail threads through every pass (fp, the table, the scratch block, the screen
// window), the v0/v1 its last pass leaves, and whether this is the guest's own field or an
// in-between over host memory.
#pragma once

#include "core.h"
#include "gte_registers.h"
#include "guest_memory_map.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/guest_terrain_screen.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace spyro::guest_terrain {

// The sector lists, as offsets from the scratch block base (VXY1, kScratchListsBelowEnd below the
// frame's scratch block end). The classification pass builds the first three; the two near passes
// defer polygons into the last two for subdivision.
inline constexpr std::uint32_t kDetailList = 0x2000; // textured sectors, near enough for detail
inline constexpr std::uint32_t kTranslucentList = 0x2200; // translucent sectors (flag 0x5000)
inline constexpr std::uint32_t kFineSplitList = 0x2300; // polygons with a vertex nearer than 0x140
inline constexpr std::uint32_t kCoarseSplitList = 0x2400; // polygons near enough to split once
inline constexpr std::uint32_t kFarList = 0x2A00;         // untextured low-detail sectors

// Every pass reuses the scratchpad for its vertex cache.
inline constexpr std::uint32_t kScratchpad = psx::kScratchpadPhysicalBase;
inline constexpr std::uint32_t kScratchpadBytes = psx::kScratchpadBytes;
inline constexpr std::uint32_t kVertexDepths =
    kScratchpad + 0x2A8u; // the detail passes' SZ halfwords
// One past the last entry of the queue of primitives too large for the GPU (ResplitFacts::queue).
inline constexpr std::uint32_t kSplitPrimitiveCursor = kScratchpad + 0x3FCu;

// The camera an in-between draws with, built by the guest's own builder from angles between two
// fields: the drawer's matrix (row 1 scaled), the classification pass's unscaled one, and the world
// position.
struct InBetweenCamera {
  psx::gte::RotationWords rotation{};
  psx::gte::RotationWords viewRotation{};
  std::uint32_t position[3]{};
  // The angle halfwords both rotations were built from.
  std::array<std::int16_t, 3> angles{};
};

// The drawer's packets stop this far above the scratch block, and the far pass's buffer ends there
// too.
inline constexpr std::uint32_t kPrimitiveArenaBytes = 0x1000;

// The guest's own field, or one between two of them over host memory with no guest call or write.
enum class FrameMode : std::uint8_t { RealField, InBetween };

// One packet's size in bytes, as the passes allocate it from the primitive cursor.
struct PacketSize {
  std::uint32_t bytes = 0;
};

// The bin's previous head (0 when empty), the packet linked, and the bin's ordinal.
struct Linked {
  std::uint32_t head = 0;
  std::uint32_t primitive = 0;
  std::uint32_t markKey = 0;
};

struct TerrainFrame {
  TerrainFrame(Core &core,
               const Facts &facts,
               const guest_render_globals::Globals &globals,
               ScreenBounds bounds,
               const psx::present::EmitMemory &memory,
               FrameMode mode)
      : core(core), facts(facts), globals(globals), bounds(bounds), memory(memory), mode(mode) {}

  // Links the packet at `primitive` as the head of `bin` in the published table and advances past
  // it.
  Linked linkAndAdvance(std::uint32_t bin, PacketSize size) {
    const std::uint32_t slot = markSlot(bin);
    if (!realField()) {
      // The guest's table past 512 bins holds the last field's chains, so an in-between starts each
      // bin empty.
      std::vector<std::uint8_t> &linked = *linkedBins;
      if (bin >= linked.size()) {
        linked.resize(bin + 1, 0);
      }
      if (linked[bin] == 0) {
        memory.mem_w32(slot, 0);
        linked[bin] = 1;
      }
      // Host packet bytes are not zeroed by the guest allocator.
      guest_memory::linkAfter(memory, primitive, 0);
    }
    const Linked linked{
        depth_bins::linkIntoBin(memory, slot, primitive, primitive), primitive, bin};
    primitive += size.bytes;
    return linked;
  }

  // Retail publishes the mark as a slot address; the ordinal is the same bin without the table's
  // address.
  [[nodiscard]] std::uint32_t markSlot(std::uint32_t bin) const {
    return orderingTable + bin * depth_bins::kBinStride;
  }
  [[nodiscard]] std::uint32_t markBinFrom(std::uint32_t slot) const {
    return (slot - orderingTable) / depth_bins::kBinStride;
  }

  [[nodiscard]] bool realField() const {
    return mode == FrameMode::RealField;
  }

  [[nodiscard]] psx::gte::RotationWords rotation() const {
    if (inBetweenCamera != nullptr) {
      return inBetweenCamera->rotation;
    }
    return guest_memory::rotationAt(psx::present::EmitMemory(core), globals.cameraRotation);
  }
  // The classification pass's bounding-sphere matrix, a second guest word beside the camera
  // rotation.
  [[nodiscard]] psx::gte::RotationWords classificationRotation() const {
    if (inBetweenCamera != nullptr) {
      return inBetweenCamera->viewRotation;
    }
    return guest_memory::rotationAt(psx::present::EmitMemory(core), facts.classify.viewRotation);
  }
  // Where the far pass stops linking: the pool's top for a real field, the budget's end for an
  // in-between.
  [[nodiscard]] std::uint32_t primitiveLimit() const {
    return inBetweenArenaBase != 0
               ? inBetweenArenaBase + static_cast<std::uint32_t>(inBetweenPacketBudget)
               : scratch + kPrimitiveArenaBytes;
  }
  [[nodiscard]] std::uint32_t primitiveBase() const {
    return inBetweenArenaBase != 0 ? inBetweenArenaBase : core.mem_r32(globals.primitiveCursor);
  }
  [[nodiscard]] std::uint32_t positionWord(std::uint32_t index) const {
    if (inBetweenCamera != nullptr) {
      return inBetweenCamera->position[index];
    }
    return core.mem_r32(globals.cameraPosition + 4 * index);
  }

  // Null for a real field, which reads the guest's own camera words.
  const InBetweenCamera *inBetweenCamera = nullptr;
  // The packet arena an in-between was given, 0 for a real field.
  std::uint32_t inBetweenArenaBase = 0;
  std::int32_t inBetweenPacketBudget = 0;
  // Set for an in-between: the bins it linked, grown to the highest it names.
  std::vector<std::uint8_t> *linkedBins = nullptr;

  Core &core;
  const Facts &facts;
  const guest_render_globals::Globals &globals;
  const ScreenBounds bounds;
  const psx::present::EmitMemory &memory;
  const FrameMode mode;
  std::uint32_t argument = 0;          // the drawer's $a0
  std::uint32_t scratch = 0;           // the scratch block base
  std::uint32_t primitive = 0;         // fp: the next free primitive packet
  std::uint32_t orderingTable = 0;     // the ordering table's bin 0
  std::uint32_t textures = 0;          // the level's 48-byte texture records
  std::uint32_t fineSplitCursor = 0;   // next free kFineSplitList word
  std::uint32_t coarseSplitCursor = 0; // next free kCoarseSplitList word
  std::uint32_t v0 = 0;
  std::uint32_t v1 = 0;
};

} // namespace spyro::guest_terrain