#pragma once

#include <cstdint>

namespace spyro {

// A press that ends a counted screen leaves `kept` of its `remaining` updates to run, at least
// `minimum`. Each update draws into the other display buffer, so an even number dropped leaves the
// guest on the buffer it would have reached, whose draw env its next screen's first clear uses.
[[nodiscard]] inline constexpr std::uint32_t keptUpdates(std::uint32_t remaining,
                                                         std::uint32_t minimum) {
  if (remaining <= minimum) {
    return remaining;
  }
  return ((remaining - minimum) & 1u) == 0u ? minimum : minimum + 1u;
}

} // namespace spyro
