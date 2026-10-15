// terrain_rebuild.h — the terrain drawer run again over host memory at a camera between two of the
// guest's, and the packets it links, walked in retail's flatten order.
//
// The state producer (terrain_state_producer.h) turns the packets into record primitives. It never
// writes guest memory and runs no guest code; it reads the words the field read, as it read them,
// and the guest for anything else.
#pragma once

#include "core.h"
#include "guest_camera_builder.h"
#include "guest_depth_bins.h"
#include "guest_packet_chain.h"
#include "guest_render_globals.h"
#include "guest_terrain_drawer.h"
#include "guest_terrain_facts.h"
#include "guest_terrain_frame.h"
#include "guest_working_memory.h"

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

// The camera at `f` between two states: angles the short way round and rebuilt into both matrices
// by the guest's own builder, positions as signed 32-bit words that wrap as the guest's do.
[[nodiscard]] InBetweenCamera cameraBetween(Core &core,
                                            const TerrainImage &image,
                                            const CameraState &from,
                                            const CameraState &to,
                                            double f);

// The sectors visible in either field, so none that enters or leaves between them is dropped.
[[nodiscard]] std::vector<std::uint8_t> unionVisibility(std::span<const std::uint8_t> from,
                                                        std::span<const std::uint8_t> to);

// What a real field hands the in-betweens drawn from it: the sectors its visibility call named,
// the guest words it read, the drawer's $a0 and the packet budget its far pass had.
struct FieldInputs {
  std::span<const std::uint8_t> visibleSectors;
  std::span<const guest_memory::LevelWord> readWords;
  std::uint32_t argument = 0;
  std::int32_t packetBudget = 0;
};

// One run of the drawer over host memory. Construction does the whole traversal; the packets are
// then walked with `forEachPacket`. The GTE is handed back as found when this is destroyed.
class Rebuild {
public:
  Rebuild(Core &core,
          const TerrainImage &image,
          const InBetweenCamera &camera,
          const FieldInputs &field);
  ~Rebuild();
  Rebuild(const Rebuild &) = delete;
  Rebuild &operator=(const Rebuild &) = delete;

  [[nodiscard]] std::uint32_t visibleSectors() const {
    return visible_;
  }

  // Calls `visit(bin, packetAddress, gp0Words)` for every packet in retail's flatten order
  // (0x8001B2A8): deepest linked bin first, each bin from its first-linked packet.
  template <typename Visit> void forEachPacket(Visit &&visit) {
    guest_memory::PacketChains chains(memory_, {arenaBase_, arenaBytes_});
    for (auto bin = static_cast<std::uint32_t>(linkedBins_->size()); bin-- > 0;) {
      if ((*linkedBins_)[bin] == 0) {
        continue;
      }
      const std::uint32_t first =
          memory_.r32(tableBase_ + bin * depth_bins::kBinStride + depth_bins::kBinFirstOffset);
      chains.walk(first == 0 ? 0u : 0x80000000u | (first & 0x1FFFFCu),
                  [&visit, bin](std::uint32_t packet, std::span<const std::uint32_t> words) {
                    visit(bin, packet, words);
                  });
    }
  }

private:
  Core &core_;
  guest_memory::HostMemory memory_;
  InBetweenCamera camera_;
  std::array<std::uint32_t, 64> gteRegistersBefore_{};
  std::uint32_t gteFlagsBefore_ = 0;
  std::optional<Drawer> drawer_;
  std::uint32_t tableBase_ = 0;
  std::uint32_t arenaBase_ = 0;
  std::uint32_t arenaBytes_ = 0;
  // The bins the traversal linked: the table's memory still holds the guest's own chains elsewhere.
  const std::vector<std::uint8_t> *linkedBins_ = nullptr;
  std::uint32_t visible_ = 0;
};

} // namespace spyro::guest_terrain
