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
#include "guest_render_globals.h"
#include "guest_terrain_drawer.h"
#include "guest_terrain_facts.h"
#include "guest_terrain_frame.h"
#include "guest_terrain_memory.h"
#include "lucent/log.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

namespace spyro::guest_terrain {

// The packet arena an in-between gets: everything above `frameTop`, the end of the last range the
// traversal owns, to the end of main RAM. The address has to be guest-shaped because a packet's
// chain link is a 24-bit main-RAM offset. `bytes == 0` is the refusal case: the caller must not
// present a truncated picture.
struct PacketArena {
  std::uint32_t base = 0;
  std::uint32_t bytes = 0;
};

// One past the last byte of the 2 MiB of main RAM, as a KSEG0 address.
inline constexpr std::uint32_t kMainRamEnd = 0x80200000u;

[[nodiscard]] inline PacketArena packetArenaWindow(std::uint32_t frameTop) {
  const std::uint32_t base = (frameTop + 0xFFFu) & ~0xFFFu;
  if (base >= kMainRamEnd) {
    return PacketArena{};
  }
  return PacketArena{base, kMainRamEnd - base};
}

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
                                            const guest_render_globals::Globals &globals,
                                            const CameraState &from,
                                            const CameraState &to,
                                            double f);

// The sectors visible in either field, so none that enters or leaves between them is dropped.
[[nodiscard]] std::vector<std::uint8_t> unionVisibility(std::span<const std::uint8_t> from,
                                                        std::span<const std::uint8_t> to);

// The guest ranges one field's traversal owns rather than reads as level data: the scratchpad, the
// scratch block up to `frameTop`, the detail pass's flag window, the ordering table and the fogged
// colours. An in-between maps each empty; a real field keeps no words from them.
struct FieldWindows {
  std::uint32_t scratch = 0;
  std::uint32_t tableBase = 0;
  std::uint32_t frameTop = 0;
  std::array<GuestRange, 5> owned{};
};

[[nodiscard]] FieldWindows
fieldWindows(Core &core, const Facts &facts, const guest_render_globals::Globals &globals);

// What a real field hands the in-betweens drawn from it: the sectors its visibility call named,
// the guest words it read and the packet budget its far pass had.
struct FieldInputs {
  std::span<const std::uint8_t> visibleSectors;
  std::span<const LevelWord> readWords;
  std::int32_t packetBudget = 0;
};

// One run of the drawer over host memory. Construction does the whole traversal; the packets are
// then walked with `forEachPacket`. The GTE is handed back as found when this is destroyed.
class Rebuild {
public:
  Rebuild(Core &core,
          const Facts &facts,
          const guest_render_globals::Globals &globals,
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
    // A cyclic chain is the only way the walk could not end; an arena holds at most this many.
    const std::uint32_t budget = arenaBytes_ / 4u + 1u;
    std::uint32_t walked = 0;
    std::array<std::uint32_t, kMaxPacketWords> words{};
    for (auto bin = static_cast<std::uint32_t>(linkedBins_->size()); bin-- > 0;) {
      std::uint32_t packet =
          (*linkedBins_)[bin] == 0
              ? 0u
              : guestAddress(memory_.r32(tableBase_ + bin * depth_bins::kBinStride +
                                         depth_bins::kBinFirstOffset));
      while (packet != 0) {
        const std::uint32_t tag = memory_.r32(packet);
        const std::uint32_t count = tag >> 24;
        if (++walked > budget || packet < arenaBase_ || count > kMaxPacketWords ||
            packet + 4u * (count + 1u) > arenaBase_ + arenaBytes_) {
          lucent::error("terrrebuild",
                        "bin {} links a {}-word packet at 0x{:08X} outside the arena "
                        "[0x{:08X},+0x{:X}) or past {} packets",
                        bin,
                        count,
                        packet,
                        arenaBase_,
                        arenaBytes_,
                        budget);
          std::abort();
        }
        // A zero count is the re-split pass's emptied primitive.
        if (count != 0) {
          for (std::uint32_t i = 0; i < count; ++i) {
            words[i] = memory_.r32(packet + 4u * (i + 1u));
          }
          visit(bin, packet, std::span<const std::uint32_t>(words.data(), count));
        }
        // The link is a main-RAM offset; zero ends the chain.
        const std::uint32_t next = tag & 0x00FFFFFFu;
        packet = next == 0 ? 0u : guestAddress(next);
      }
    }
  }

private:
  Core &core_;
  HostMemory memory_;
  InBetweenCamera camera_;
  std::array<std::uint32_t, 64> gteRegistersBefore_{};
  std::uint32_t gteFlagsBefore_ = 0;
  std::optional<Drawer> drawer_;
  // The GP0 FIFO is 256 words, so no packet is longer.
  static constexpr std::uint32_t kMaxPacketWords = 256;
  static constexpr std::uint32_t guestAddress(std::uint32_t word) {
    return 0x80000000u | (word & 0x1FFFFCu);
  }
  std::uint32_t tableBase_ = 0;
  std::uint32_t arenaBase_ = 0;
  std::uint32_t arenaBytes_ = 0;
  // The bins the traversal linked: the table's memory still holds the guest's own chains elsewhere.
  const std::vector<std::uint8_t> *linkedBins_ = nullptr;
  std::uint32_t visible_ = 0;
};

} // namespace spyro::guest_terrain
