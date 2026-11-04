// guest_word.h — MIPS word arithmetic as the hand-written render routines spell it.
#pragma once

#include <array>
#include <cstdint>

namespace spyro::guest_word {

[[nodiscard]] constexpr std::int32_t asSigned(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

[[nodiscard]] constexpr std::uint32_t asWord(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

// `sra value, bits`.
[[nodiscard]] constexpr std::uint32_t sra(std::uint32_t value, std::uint32_t bits) {
  return static_cast<std::uint32_t>(asSigned(value) >> bits);
}

// The magnitude of a signed word, as `bgez`/`negu` take it.
[[nodiscard]] constexpr std::uint32_t absolute(std::uint32_t value) {
  return asSigned(value) < 0 ? 0u - value : value;
}

// The low halfword sign-extended, as `lh` loads it.
[[nodiscard]] constexpr std::uint32_t signedHalf(std::uint32_t word) {
  return sra(word << 16, 16);
}

// A packed 11/11/10 vertex delta word's fields, high first, each sign-extended.
[[nodiscard]] constexpr std::array<std::uint32_t, 3> packedFields(std::uint32_t word) {
  return {sra(word, 21), sra(word << 11, 21), sra(word << 22, 21)};
}

} // namespace spyro::guest_word
