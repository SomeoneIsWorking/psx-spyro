// guest_packet_chain.h — the packets an in-between linked into its arena, walked along their tag
// links, and the draw mode they set: every producer that reruns a guest render routine over host
// memory reads its output here.
#pragma once

#include "gp0_primitive_decode.h"
#include "lucent/log.h"
#include "render/field/guest_working_memory.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

namespace spyro::guest_memory {

class PacketChains {
public:
  PacketChains(const WorkingMemory &memory, const PacketArena &arena)
      : memory_(memory), arena_(arena) {}

  // Calls `visit(packet, gp0Words)` for each packet from `first` to the zero link. A packet outside
  // the arena, or more packets than the arena holds, is a broken chain and aborts.
  template <typename Visit> void walk(std::uint32_t first, Visit &&visit) {
    const std::uint32_t budget = arena_.bytes / 4u + 1u;
    std::array<std::uint32_t, kMaxPacketWords> words{};
    for (std::uint32_t packet = first; packet != 0;) {
      const std::uint32_t tag = memory_.r32(packet);
      const std::uint32_t count = tag >> 24;
      if (++walked_ > budget || packet - arena_.base >= arena_.bytes ||
          packet + 4u * (count + 1u) > arena_.base + arena_.bytes) {
        lucent::error(
            "packetchain",
            "a {}-word packet at 0x{:08X} outside the arena [0x{:08X},+0x{:X}) or past {} packets",
            count,
            packet,
            arena_.base,
            arena_.bytes,
            budget);
        std::abort();
      }
      // A zero count is an emptied packet.
      if (count != 0) {
        for (std::uint32_t i = 0; i < count; ++i) {
          words[i] = memory_.r32(packet + 4u * (i + 1u));
        }
        visit(packet, std::span<const std::uint32_t>(words.data(), count));
      }
      // The link is a main-RAM offset; zero ends the chain.
      const std::uint32_t next = tag & 0x00FFFFFFu;
      packet = next == 0 ? 0u : 0x80000000u | (next & 0x1FFFFCu);
    }
  }

private:
  // The GP0 FIFO is 256 words, so no packet is longer.
  static constexpr std::uint32_t kMaxPacketWords = 256;

  const WorkingMemory &memory_;
  PacketArena arena_;
  std::uint32_t walked_ = 0;
};

// The draw environment's texture page as a chain's GP0 E1 packets set it: what an untextured
// primitive after one is blended with, in submission order.
class DrawMode {
public:
  // True when `words` is a draw-mode packet, which is then taken.
  bool take(std::span<const std::uint32_t> words) {
    if (words[0] >> 24 != 0xE1u) {
      return false;
    }
    attribute_ = static_cast<std::uint16_t>(words[0]);
    return true;
  }
  // Leaves a primitive alone until a packet has set the mode.
  void apply(psx::present::DrawPrimitive &primitive) const {
    if (attribute_ && !primitive.textured) {
      psx::gpu::applyTexPageAttribute(primitive.state, *attribute_);
    }
  }

private:
  std::optional<std::uint16_t> attribute_;
};

} // namespace spyro::guest_memory
