// guest_working_memory.h — what Spyro's render routines share beside psxport's EmitMemory: the
// arena an in-between links its packets into, a packet's tag link, and the draw a field saves for
// its in-betweens.
#pragma once

#include "core.h"
#include "emit_memory.h"
#include "gte_registers.h"
#include "guest_address_range.h"
#include "guest_memory_map.h"
#include "lucent/log.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace spyro::guest_memory {

// Where an in-between links its packets, guest-shaped because a tag's link is a 24-bit main-RAM
// offset. No bytes when nothing is free above the field's ranges.
using PacketArena = GuestSpan;

// From the page above `frameTop`, the end of the last range the traversal owns, to the end of main
// RAM.
[[nodiscard]] constexpr PacketArena packetArenaWindow(std::uint32_t frameTop) {
  const std::uint32_t base = (frameTop + 0xFFFu) & ~0xFFFu;
  if (base >= psx::kMainRamEnd) {
    return PacketArena{};
  }
  return PacketArena{base, psx::kMainRamEnd - base};
}

// The tag of `packet` linked to `next`: its low 24 bits, the word count kept.
inline void
linkAfter(const psx::present::EmitMemory &memory, std::uint32_t packet, std::uint32_t next) {
  memory.mem_w16(packet, static_cast<std::uint16_t>(next));
  memory.mem_w8(packet + 2, static_cast<std::uint8_t>(next >> 16));
}

// The five rotation words at `address`.
[[nodiscard]] inline psx::gte::RotationWords rotationAt(const psx::present::EmitMemory &memory,
                                                        std::uint32_t address) {
  psx::gte::RotationWords words{};
  memory.readWords(address, words);
  return words;
}

} // namespace spyro::guest_memory
