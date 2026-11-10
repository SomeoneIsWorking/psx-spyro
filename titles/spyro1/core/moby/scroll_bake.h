// scroll_bake.h — a texture scroll node's UV bytes: 0x8002A6FC's second list steps each node's
// 7-bit position (+6) and bakes it into the narrow (low detail) and wide (high detail) texture
// records at its index (+4), which the terrain draw reads.
#pragma once

#include <cstdint>

namespace spyro1::scroll {

// 0x8002A6FC's node lists, each a count then the node pointer array; the scroll list is the second.
inline constexpr std::uint32_t kNodeLists = 0x80078560u;
inline constexpr std::uint32_t kNodeListBytes = 8u;
inline constexpr std::uint32_t kNodeList = kNodeLists + kNodeListBytes;
inline constexpr std::uint32_t kNodeIndex = 0x04u;
inline constexpr std::uint32_t kNodePosition = 0x06u;
inline constexpr std::uint32_t kPositionBits = 7u;
// Pointers to the record tables.
inline constexpr std::uint32_t kNarrowRecords = 0x800785C0u;
inline constexpr std::uint32_t kWideRecords = 0x800785C4u;
inline constexpr std::uint32_t kNarrowRecordSize = 0x10u;
inline constexpr std::uint32_t kWideRecordSize = 0xA8u;

// Wide-record runs of the half position plus a bias, four bytes apart.
struct ByteRun {
  std::uint32_t offset;
  std::uint32_t count;
  std::uint32_t bias;
  bool masked;
};
inline constexpr ByteRun kWideRuns[] = {
    {0x19u, 4u, 0x20u, true},
    {0x29u, 8u, 0x00u, false},
    {0x49u, 8u, 0x10u, true},
    {0x69u, 8u, 0x20u, true},
    {0x89u, 8u, 0x30u, true},
};
inline constexpr std::uint32_t kWideHalves[] = {0x09u, 0x0Du, 0x11u, 0x15u};

// The bytes `position` bakes, in the guest's store order: `narrow(offset, byte)` and `wide(offset,
// byte)` are offsets into the node's two records.
template <typename Narrow, typename Wide>
void bake(std::uint32_t position, Narrow narrow, Wide wide) {
  const auto quarter = static_cast<std::uint8_t>(position >> 2);
  for (std::uint32_t offset = 0x01u; offset <= 0x0Du; offset += 4u) {
    narrow(offset, quarter);
  }
  wide(0x01u, quarter);
  wide(0x05u, quarter);
  const std::uint32_t half = position >> 1;
  for (const std::uint32_t offset : kWideHalves) {
    wide(offset, static_cast<std::uint8_t>(half));
  }
  for (const ByteRun &run : kWideRuns) {
    std::uint32_t value = half + run.bias;
    if (run.masked) {
      value &= 0x3Fu;
    }
    for (std::uint32_t i = 0u; i < run.count; ++i) {
      wide(run.offset + (i * 4u), static_cast<std::uint8_t>(value));
    }
  }
}

} // namespace spyro1::scroll
