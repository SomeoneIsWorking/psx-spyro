#include "spyro1/core/moby/native_moby_animation.h"

#include "core/native_execution.h"
#include "guest_call.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kAngDiff8;
using spyro1::guest::kFrameLag;

constexpr std::uint32_t kMobyAnimationFrame = 0x46u;
constexpr std::uint32_t kFifthArgumentSlot = 0x10u;
constexpr std::uint32_t kLagThreeFields = 3u;
constexpr std::uint32_t kLagFourFields = 4u;
// The frame an animation reaches from frame a1 by a2, with a3 the wrap threshold; a leaf.
constexpr std::uint32_t kAdvanceAnimationFrame = 0x800179F0u; // `jal` at 0x80038F70 and 0x80038F94

// The fourth argument is the frame word `sra 1` + 1, rounding towards minus infinity.
void setMobyAnimationFrame(Core *c,
                           const std::uint32_t animation,
                           const std::uint32_t moby,
                           const std::uint32_t frame) {
  const std::uint32_t half = static_cast<std::uint32_t>(static_cast<std::int32_t>(frame) >> 1);
  psx::cpu::callGuestNow(*c,
                         "advance_moby_animation",
                         kAdvanceAnimationFrame,
                         animation,
                         c->mem_r8(moby + kMobyAnimationFrame),
                         frame,
                         half + 1u);
  c->mem_w8(moby + kMobyAnimationFrame, static_cast<std::uint8_t>(c->r[2]));
}

// 0x80038EE0: lag 3 scales the step by 1.5 and lag 4 by 2; the stack argument gates the wrap.
void advanceMobyAnimation(Core *c) {
  const std::uint32_t moby = c->r[4];
  const std::uint32_t animation = c->r[5];
  const std::uint32_t step = c->r[6];
  const std::uint32_t limit = c->r[7];
  const std::uint32_t wrapRequested = c->mem_r32(c->r[29] + kFifthArgumentSlot);

  psx::cpu::callGuestNow(*c,
                         "advance_moby_animation",
                         kAngDiff8,
                         animation,
                         c->mem_r8(moby + kMobyAnimationFrame));
  const std::uint32_t length = c->r[2];

  const std::uint32_t lag = c->mem_r32(kFrameLag);
  c->r[3] = lag;
  std::uint32_t frame = step;
  if (lag == kLagThreeFields) {
    frame = step + static_cast<std::uint32_t>(static_cast<std::int32_t>(step) >> 1);
  } else if (lag == kLagFourFields) {
    frame = step << 1;
  }
  if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(frame)) {
    frame = length;
  }
  if (static_cast<std::int32_t>(length) >= static_cast<std::int32_t>(limit)) {
    setMobyAnimationFrame(c, animation, moby, frame);
    c->r[2] = 0;
    return;
  }
  if (wrapRequested != 0u) {
    setMobyAnimationFrame(c, animation, moby, frame);
  }
  c->r[2] = 1;
}

} // namespace

void registerMobyAnimationOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x80038EE0u, "advance_moby_animation", advanceMobyAnimation);
}

} // namespace spyro1::native
