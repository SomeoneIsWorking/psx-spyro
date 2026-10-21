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

// A saved draw: a trivially copyable header, then the level words the draw read, packed LevelWords
// viewed in the saved state.
template <typename Header> struct SavedDraw {
  Header header;
  std::span<const std::byte> reads;
};

// The draw saved under the open emission scope: the header, then `reads` and `moreReads`.
template <typename Header>
void saveDraw(Core &core,
              const Header &header,
              std::span<const psx::present::LevelWord> reads,
              std::span<const psx::present::LevelWord> moreReads = {}) {
  static_assert(std::is_trivially_copyable_v<Header>);
  const std::array<std::span<const std::byte>, 3> parts{
      std::as_bytes(std::span<const Header, 1>(&header, 1)),
      std::as_bytes(reads),
      std::as_bytes(moreReads)};
  core.frameStates.save(core.emission.current(), parts);
}

// Aborts naming `what` when `bytes` is not a header and whole words.
template <typename Header>
[[nodiscard]] SavedDraw<Header> unpackDraw(std::span<const std::byte> bytes, const char *what) {
  static_assert(std::is_trivially_copyable_v<Header>);
  constexpr std::size_t kWordBytes = sizeof(psx::present::LevelWord);
  if (bytes.size() < sizeof(Header) || (bytes.size() - sizeof(Header)) % kWordBytes != 0) {
    lucent::error("guestmem", "a saved {} of {} bytes is not one", what, bytes.size());
    std::abort();
  }
  SavedDraw<Header> saved;
  std::memcpy(&saved.header, bytes.data(), sizeof(Header));
  saved.reads = bytes.subspan(sizeof(Header));
  return saved;
}

} // namespace spyro::guest_memory
