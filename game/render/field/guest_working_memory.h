// guest_working_memory.h — what Spyro's render routines share beside psxport's EmitMemory: a
// packet's tag link and a rotation read.
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
