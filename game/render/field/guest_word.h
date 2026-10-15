// guest_word.h — MIPS word arithmetic as the hand-written render routines spell it.
#pragma once

#include <cstdint>

namespace spyro::guest_word {

[[nodiscard]] constexpr std::int32_t asSigned(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

// `sra value, bits`.
[[nodiscard]] constexpr std::uint32_t sra(std::uint32_t value, std::uint32_t bits) {
  return static_cast<std::uint32_t>(asSigned(value) >> bits);
}

// `div n, d` then `mflo`: by zero 1 for a negative dividend and -1 otherwise, as the R3000 does.
[[nodiscard]] constexpr std::uint32_t divLo(std::uint32_t n, std::uint32_t d) {
  if (d == 0) {
    return asSigned(n) < 0 ? 1u : 0xFFFFFFFFu;
  }
  if (n == 0x80000000u && d == 0xFFFFFFFFu) {
    return n;
  }
  return static_cast<std::uint32_t>(asSigned(n) / asSigned(d));
}

} // namespace spyro::guest_word
