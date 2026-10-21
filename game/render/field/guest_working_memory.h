// guest_working_memory.h — what Spyro's render routines share beside psxport's EmitMemory: the arena an
// in-between links its packets into, a packet's tag link, and the draw a field saves for its in-betweens.
#pragma once

#include "core.h"
#include "emit_memory.h"
#include "guest_address_range.h"
#include "guest_memory_map.h"
#include "gte_registers.h"
#include "lucent/log.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace spyro::guest_memory {

// Where an in-between links its packets, guest-shaped because a tag's link is a 24-bit main-RAM offset.
// No bytes when nothing is free above the field's ranges.
using PacketArena = GuestSpan;

// From the page above `frameTop`, the end of the last range the traversal owns, to the end of main RAM.
[[nodiscard]] constexpr PacketArena packetArenaWindow(std::uint32_t frameTop) {
  const std::uint32_t base = (frameTop + 0xFFFu) & ~0xFFFu;
  if (base >= psx::kMainRamEnd) {
    return PacketArena{};
  }
  return PacketArena{base, psx::kMainRamEnd - base};
}

// The tag of `packet` linked to `next`: its low 24 bits, the word count kept.
inline void linkAfter(const psx::present::EmitMemory &memory, std::uint32_t packet, std::uint32_t next) {
  memory.mem_w16(packet, static_cast<std::uint16_t>(next));
  memory.mem_w8(packet + 2, static_cast<std::uint8_t>(next >> 16));
}

// The five rotation words at `address`.
[[nodiscard]] inline psx::gte::RotationWords rotationAt(const psx::present::EmitMemory &memory, std::uint32_t address) {
  psx::gte::RotationWords words{};
  memory.readWords(address, words);
  return words;
}

// A saved draw: a trivially copyable header, then the level words the draw read.
template <typename Header> struct SavedDraw {
  Header header;
  std::vector<psx::present::LevelWord> reads;
};

template <typename Header>
[[nodiscard]] std::vector<std::byte> packDraw(const Header &header, std::span<const psx::present::LevelWord> reads) {
  static_assert(std::is_trivially_copyable_v<Header>);
  std::vector<std::byte> bytes(sizeof(Header) + reads.size() * sizeof(psx::present::LevelWord));
  std::memcpy(bytes.data(), &header, sizeof(Header));
  std::memcpy(bytes.data() + sizeof(Header), reads.data(), reads.size() * sizeof(psx::present::LevelWord));
  return bytes;
}

// The draw saved under the open emission scope.
template <typename Header>
void saveDraw(Core &core, const Header &header, std::span<const psx::present::LevelWord> reads) {
  const std::vector<std::byte> bytes = packDraw(header, reads);
  core.frameStates.save(core.emission.current(), std::span<const std::byte>(bytes));
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
  saved.reads.resize((bytes.size() - sizeof(Header)) / kWordBytes);
  std::memcpy(saved.reads.data(), bytes.data() + sizeof(Header), saved.reads.size() * kWordBytes);
  return saved;
}

} // namespace spyro::guest_memory
