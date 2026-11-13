#include "spyro3/core/loader_checksum.h"

#include "core.h"
#include "core/loader_drain.h"
#include "guest_abi.h"
#include "guest_call_site.h"
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
// Stage 4 (the overlay step 0x8007494C) steps 0x8006C5E4 once a call before it moves on.
constexpr std::int32_t kStageTransfers = 4;
constexpr std::uint32_t kTransferStep = 0x8006C5E4u;
// The loading screen's own call, state 5 sub-state 1 in 0x80058778; it leaves once the stage is -1.
constexpr psx::cpu::GuestCallSite kLoadingScreenCall =
    psx::cpu::GuestCallSite::fromJal(0x800588A0u, kLoader);

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

std::int32_t stage(Core &core) {
  return static_cast<std::int32_t>(core.mem_r32(kStage));
}

// Where the load stands: the stage, stage 4's own steps (two VRAM halves, the SPU chunks, the next
// read) and -1, the end, past every stage.
std::int64_t loadProgress(Core &core) {
  const std::int32_t at = stage(core);
  if (at < 0) {
    return std::int64_t{1} << 32;
  }
  const std::int64_t step = at == kStageTransfers ? core.mem_r32(kTransferStep) : 0u;
  return (static_cast<std::int64_t>(at) << 8) + step;
}

void completeChecksum(Core &core) {
  if (core.mem_r32(kStage) - 2u < 6u) {
    int calls = 0;
    for (; calls < kCallLimit && core.mem_r32(kCursor) != kDone; ++calls) {
      checksumCall(core);
    }
    if (core.mem_r32(kCursor) != kDone) {
      lucent::error("load",
                    "Spyro 3's loader checksum did not end after {} calls (cursor 0x{:08X})",
                    calls,
                    core.mem_r32(kCursor));
      std::abort();
    }
  }
}

// One loader call as the guest makes it, the checksum its read polls would have run completed
// first.
bool loaderCall(Core &core) {
  completeChecksum(core);
  return psx::cpu::callOriginalOrPropagate(core, kLoader);
}

void loader(Core *core) {
  if (core->r[psx::cpu::kRa] != kLoadingScreenCall.returnPc) {
    (void)loaderCall(*core);
    return;
  }
  const std::uint32_t returnPc = core->r[psx::cpu::kRa];
  bool returned = true;
  spyro::drainLoader(
      [core, returnPc, &returned] {
        // The completion of the read the last call issued is due at once; the guest takes it before
        // its next poll.
        psx::cpu::takePendingInterrupts(*core);
        core->r[psx::cpu::kRa] = returnPc;
        returned = loaderCall(*core);
      },
      [core] {
        return loadProgress(*core);
      },
      [core, &returned] {
        return returned && stage(*core) >= 0;
      });
}

} // namespace

void installLoaderChecksum(Core &core) {
  psx::cpu::installNativeOverride(core, kLoader, "spyro3-loader-checksum", loader);
}

} // namespace spyro3
