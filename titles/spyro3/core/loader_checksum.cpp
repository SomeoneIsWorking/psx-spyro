#include "spyro3/core/loader_checksum.h"

#include "core.h"
#include "native_dispatch.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro3 {
namespace {

constexpr std::uint32_t kLoader = 0x8002CA50u;
constexpr std::uint32_t kStage = 0x8006C518u;
constexpr std::uint32_t kCursor = 0x8006C6B8u;
constexpr std::uint32_t kSum = 0x8006C6B4u;
// The bytes are read 0x473E past the cursor.
constexpr std::uint32_t kByteOffset = 0x473Eu;
// First range: the cursor steps 1..4 bytes by the sum.
constexpr std::uint32_t kFirstStart = 0x8000D8CFu;
constexpr std::uint32_t kFirstEnd = 0x8006085Eu;
// Second range: one byte a step, then the end mark stage 7 waits for.
constexpr std::uint32_t kSecondStart = 0x8007020Eu;
constexpr std::uint32_t kSecondEnd = 0x80076FA6u;
constexpr std::uint32_t kDone = 0x8007B6E4u;
constexpr int kStepsPerCall = 0x1400;
// Far more calls than the two ranges take at kStepsPerCall.
constexpr int kCallLimit = 256;

std::uint32_t mixByte(std::uint32_t sum, std::uint32_t byte) {
  sum ^= byte << 15;
  for (int bit = 0; bit < 8; ++bit) {
    sum <<= 1;
    if ((sum & 0x800000u) != 0) {
      sum ^= 0x400280u;
    }
  }
  return sum;
}

// 0x8002CB00..0x8002CC9C: one call's worth of the checksum.
void checksumCall(Core &core) {
  std::uint32_t cursor = core.mem_r32(kCursor);
  std::uint32_t sum = core.mem_r32(kSum);
  if (cursor == 0) {
    cursor = kFirstStart;
  }
  if (cursor < kFirstEnd) {
    for (int step = 0; step < kStepsPerCall && cursor < kFirstEnd; ++step) {
      sum = mixByte(sum, core.mem_r8(cursor + kByteOffset));
      cursor += ((sum & 0x180u) >> 7) + 1;
    }
    if (cursor >= kFirstEnd) {
      cursor = kSecondStart;
    }
  }
  if (cursor >= kSecondStart) {
    for (int step = 0; step < kStepsPerCall && cursor < kSecondEnd; ++step) {
      sum = mixByte(sum, core.mem_r8(cursor + kByteOffset));
      ++cursor;
    }
    if (cursor == kSecondEnd) {
      cursor = kDone;
    }
  }
  core.mem_w32(kCursor, cursor);
  core.mem_w32(kSum, sum);
}

void loader(Core *core) {
  if (core->mem_r32(kStage) - 2u < 6u) {
    int calls = 0;
    for (; calls < kCallLimit && core->mem_r32(kCursor) != kDone; ++calls) {
      checksumCall(*core);
    }
    if (core->mem_r32(kCursor) != kDone) {
      lucent::error("load",
                    "Spyro 3's loader checksum did not end after {} calls (cursor 0x{:08X})",
                    calls,
                    core->mem_r32(kCursor));
      std::abort();
    }
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, kLoader);
}

} // namespace

void installLoaderChecksum(Core &core) {
  psx::cpu::installNativeOverride(core, kLoader, "spyro3-loader-checksum", loader);
}

} // namespace spyro3
