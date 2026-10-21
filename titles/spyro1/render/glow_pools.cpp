#include "spyro1/render/glow_pools.h"

#include "core.h"
#include "core/native_execution.h"
#include "spyro1/render/glows.h"

#include <cstdint>

namespace spyro1::glows {
namespace {

constexpr std::uint32_t kNoFreeSparkle = 0xFFFFFFFFu;

// 0x8005882C: the first sparkle whose fade byte is zero, or -1; v1 keeps the byte the scan stopped on.
void findFreeSparkle(Core *c) {
  std::uint32_t fade = 0;
  std::uint32_t slot = 0;
  for (; slot < kSparkleCount; ++slot) {
    fade = c->mem_r8(kSparkles + slot * kSparkleBytes + sparkle::kFade);
    if (fade == 0u) {
      break;
    }
  }
  c->r[3] = fade;
  c->r[2] = slot < kSparkleCount ? slot : kNoFreeSparkle;
}

// 0x80058B68: every emitter's count and every sparkle's fade cleared; v0 exits one stride below zero.
void clearPools(Core *c) {
  for (std::uint32_t index = 0; index < kEmitterCount; ++index) {
    c->mem_w32(kEmitters + index * kEmitterBytes + emitter::kCount, 0);
  }
  for (std::uint32_t slot = 0; slot < kSparkleCount; ++slot) {
    c->mem_w8(kSparkles + slot * kSparkleBytes + sparkle::kFade, 0);
  }
  c->r[2] = 0u - kSparkleBytes;
}

} // namespace

void registerPoolOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x8005882Cu, "find_free_sparkle_slot", findFreeSparkle);
  psx::cpu::installNativeOverride(core, 0x80058B68u, "clear_glows_and_sparkles", clearPools);
}

} // namespace spyro1::glows
