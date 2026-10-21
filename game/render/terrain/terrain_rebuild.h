// terrain_rebuild.h — the terrain drawer run again over host memory at a camera between two of the
// guest's, and the packets it links, walked in retail's flatten order.
//
// The state producer (terrain_state_producer.h) turns the packets into record primitives. It never
// writes guest memory and runs no guest code; it reads the words the field read, as it read them,
// and the guest for anything else.
#pragma once

#include "core.h"
#include "emit_memory.h"
#include "gte_control.h"
#include "packet_chains.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_camera_builder.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_drawer.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/guest_terrain_frame.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace spyro::guest_terrain {

// The guest's camera state the drawer's camera is built from: the three angles its builder reads
// (0x8001C2F8) and the world position, each one signed 32-bit fixed-point word.
struct CameraState {
  std::int32_t angles[3] = {};
  std::uint32_t position[3] = {};
};

// The camera the guest's globals describe now.
[[nodiscard]] CameraState readCamera(Core &core, const guest_render_globals::Globals &globals);

// The camera at `t` between two states: angles the short way round and rebuilt into both matrices
// by the guest's own builder, positions as signed 32-bit words that wrap as the guest's do.
[[nodiscard]] InBetweenCamera cameraBetween(
    Core &core, const TerrainImage &image, const CameraState &from, const CameraState &to, float t);

// The sectors visible in either field, so none that enters or leaves between them is dropped,
// written to the front of `into`.
[[nodiscard]] std::span<const std::uint8_t> unionVisibility(std::span<const std::uint8_t> from,
                                                            std::span<const std::uint8_t> to,
                                                            std::span<std::uint8_t> into);

// What a real field hands the in-betweens drawn from it: the sectors its visibility call named,
// the guest words it read, the drawer's $a0 and the packet budget its far pass had.
struct FieldInputs {
  std::span<const std::uint8_t> visibleSectors;
  std::span<const std::byte> readWords; // packed LevelWords
  std::uint32_t argument = 0;
  std::int32_t packetBudget = 0;
};

// One run of the drawer over host memory; construction does the whole traversal. The GTE is handed
// back as found when this is destroyed.
class Rebuild {
public:
  // The traversal declares its packets' unrounded vertex places to `places`, which `emit` is then
  // given.
  Rebuild(Core &core,
          const TerrainImage &image,
          const InBetweenCamera &camera,
          const FieldInputs &field,
          psx::present::PrimitiveSink &places);
  Rebuild(const Rebuild &) = delete;
  Rebuild &operator=(const Rebuild &) = delete;

  [[nodiscard]] std::uint32_t visibleSectors() const {
    return visible_;
  }

  // The packets in retail's flatten order (0x8001B2A8): deepest linked bin first, each from its
  // first packet.
  void emit(psx::present::PrimitiveSink &sink) const;

private:
  psx::present::GteGuard gte_;
  psx::present::Kept<psx::present::HostMemory>::Use host_;
  psx::present::EmitMemory memory_;
  InBetweenCamera camera_;
  std::optional<Drawer> drawer_;
  std::uint32_t tableBase_ = 0;
  GuestSpan arena_;
  // The bins the traversal linked: the table's memory still holds the guest's own chains elsewhere.
  psx::present::Kept<std::vector<std::uint8_t>>::Use linkedBins_;
  std::uint32_t visible_ = 0;
};

} // namespace spyro::guest_terrain
