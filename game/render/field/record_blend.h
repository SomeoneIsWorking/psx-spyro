// record_blend.h — a saved guest record's little-endian fields, and the blends an in-between moves
// them with.
#pragma once

#include "blend.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace spyro::record_blend {

template <typename Value, std::size_t N>
Value load(const std::array<std::uint8_t, N> &bytes, std::uint32_t at) {
  Value value;
  std::memcpy(&value, bytes.data() + at, sizeof(Value));
  return value;
}

template <typename Value, std::size_t N>
void store(std::array<std::uint8_t, N> &bytes, std::uint32_t at, Value value) {
  std::memcpy(bytes.data() + at, &value, sizeof(Value));
}

// The signed word at `at` of two records, at `t`, into `shown`.
template <std::size_t N>
void wordBetween(std::array<std::uint8_t, N> &shown,
                 const std::array<std::uint8_t, N> &from,
                 const std::array<std::uint8_t, N> &to,
                 std::uint32_t at,
                 float t) {
  store(shown,
        at,
        psx::present::lerpWord(load<std::uint32_t>(from, at), load<std::uint32_t>(to, at), t));
}

// An angle byte, 256 steps a turn, at `t` the short way round.
[[nodiscard]] inline std::uint8_t angleBetween(std::uint8_t from, std::uint8_t to, float t) {
  return static_cast<std::uint8_t>(psx::present::lerpTurn(from, to, 8, t));
}

} // namespace spyro::record_blend
